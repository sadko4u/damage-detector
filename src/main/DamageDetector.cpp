/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of damage-detector
 * Created on: 17 апр. 2024 г.
 *
 * damage-detector is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * damage-detector is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with damage-detector. If not, see <https://www.gnu.org/licenses/>.
 */

#include <private/DamageDetector.h>

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>

namespace dd
{
    static constexpr size_t TMP_BUFFER_SIZE     = 0x400;

    DamageDetector::DamageDetector(size_t channels)
    {
        vChannels                   = NULL;
        vBuffer                     = NULL;
        nTimestamp                  = 0;
        nChannels                   = channels;
        nSampleRate                 = 44100;
        nDetectTime                 = 0;
        nBounceTime                 = 0;
        nEstimateTime               = 0;
        fDetectTime                 = DFL_DETECT_TIME;
        fThresholdDB                = DFL_THRESHOLD;
        fThreshold                  = 0.0f;
        fReactivity                 = DFL_REACTIVITY;
        fEstimateTime               = DFL_ESTIMATE_TIME;
        bBypass                     = true;
        bUpdate                     = true;

        const size_t szof_channels  = lsp::align_size(channels * sizeof(channel_t), DEFAULT_ALIGN);
        const size_t szof_buffer    = lsp::align_size(sizeof(float) * TMP_BUFFER_SIZE, DEFAULT_ALIGN);
        const size_t szof_evbuf     = lsp::align_size(MAX_EVENTS * sizeof(event_t), DEFAULT_ALIGN);

        const size_t to_alloc       =
            szof_channels +
            szof_buffer +
            szof_evbuf * channels;

        uint8_t *ptr                = lsp::alloc_aligned<uint8_t>(pData, to_alloc, DEFAULT_ALIGN);
        if (ptr == NULL)
            return;

        vChannels                   = lsp::advance_ptr_bytes<channel_t>(ptr, szof_channels);
        vBuffer                     = lsp::advance_ptr_bytes<float>(ptr, szof_buffer);

        for (size_t i=0; i<channels; ++i)
        {
            channel_t *c                = &vChannels[i];

            c->sSC.construct();
            c->sSC.init(1, MAX_REACTIVITY);
            c->sSC.set_mode(lsp::dspu::SCM_RMS);
            c->sSC.set_source(lsp::dspu::SCS_MIDDLE);
            c->sSC.set_sample_rate(nSampleRate);

            c->sEvBuf.nHead             = 0.0f;
            c->sEvBuf.nTail             = 0.0f;
            c->sEvBuf.nCount            = 0.0f;

            c->nOpenTime                = 0;
            c->nCloseTime               = 0;
            c->nRaiseTime               = 0;
            c->nFallTime                = 0;
            c->nEvents                  = 0;
            c->enState                  = TRG_CLOSED;

            c->sEvBuf.vData             = lsp::advance_ptr_bytes<event_t>(ptr, szof_evbuf);
            c->sEvBuf.nHead             = 0;
            c->sEvBuf.nTail             = 0;
            c->sEvBuf.nCount            = 0;

            clear_event_buf(&c->sEvBuf);
        }
    }

    DamageDetector::~DamageDetector()
    {
        lsp::free_aligned(pData);
    }

    void DamageDetector::clear_event_buf(event_buf_t *buf)
    {
        buf->nHead              = 0;
        buf->nTail              = 0;
        buf->nCount             = 0;

        for (size_t i=0; i<MAX_EVENTS; ++i)
            buf->vData[i].nTimestamp    = 0;
    }

    size_t DamageDetector::push_event(event_buf_t *buf, timestamp_t ts)
    {
        // Drop last event if we don't have too much space
        if (buf->nCount >= MAX_EVENTS)
            buf->nTail = (buf->nTail + 1) % MAX_EVENTS;
        else
            ++buf->nCount;

        // Push event to the buffer
        buf->vData[buf->nHead].nTimestamp   = ts;
        buf->nHead = (buf->nHead + 1) % MAX_EVENTS;

        update_event_buf(buf, ts);

        // Return actual number of events in the buffer at this moment
        return buf->nCount;
    }

    void DamageDetector::update_event_buf(event_buf_t *buf, timestamp_t ts)
    {
        // Drop events that are too late relative to the current time
        while (buf->nCount > 0)
        {
            const size_t ev_ts = buf->vData[buf->nTail].nTimestamp;
            if ((ev_ts + nEstimateTime) >= ts)
                break;

            // Remove this event
            --buf->nCount;
            buf->nTail = (buf->nTail + 1) % MAX_EVENTS;
        }
    }

    void DamageDetector::update_settings()
    {
        if (!bUpdate)
            return;

        fThreshold      = lsp::dspu::db_to_gain(fThresholdDB);
        nDetectTime     = lsp::dspu::seconds_to_samples(nSampleRate, fDetectTime);
        nEstimateTime   = lsp::dspu::seconds_to_samples(nSampleRate, fEstimateTime);
        nBounceTime     = lsp::dspu::millis_to_samples(nSampleRate, fReactivity * 0.1f);

        for (size_t i=0; i<nChannels; ++i)
        {
            channel_t *c                = &vChannels[i];
            c->sSC.set_reactivity(fReactivity);
        }
    }

    void DamageDetector::set_sample_rate(size_t sample_rate)
    {
        if (sample_rate == nSampleRate)
            return;

        nTimestamp      = 0;
        nSampleRate     = sample_rate;
        for (size_t i=0; i<nChannels; ++i)
        {
            channel_t *c                = &vChannels[i];

            c->sSC.set_sample_rate(nSampleRate);
            clear_event_buf(&c->sEvBuf);
        }

        bUpdate         = true;
    }

    void DamageDetector::set_detect_time(float detect_time)
    {
        detect_time     = lsp::lsp_limit(detect_time, MIN_DETECT_TIME, MAX_DETECT_TIME);
        if (fDetectTime == detect_time)
            return;
        fDetectTime     = detect_time;
        bUpdate         = true;
    }

    void DamageDetector::set_estimation_time(float est_time)
    {
        est_time        = lsp::lsp_limit(est_time, MIN_ESTIMATE_TIME, MAX_ESTIMATE_TIME);
        if (fEstimateTime == est_time)
            return;
        fEstimateTime   = est_time;
        bUpdate         = true;
    }

    void DamageDetector::set_bypass(bool bypass)
    {
        bBypass         = bypass;
    }

    void DamageDetector::set_threshold(float thresh)
    {
        thresh          = lsp::lsp_limit(thresh, MIN_THRESHOLD, MAX_THRESHOLD);
        if (fThresholdDB == thresh)
            return;

        fThresholdDB    = thresh;
        bUpdate         = true;
    }

    void DamageDetector::bind_input(size_t channel, const float *ptr)
    {
        if (channel >= nChannels)
            return;
        vChannels[channel].vIn      = ptr;
    }

    void DamageDetector::bind_output(size_t channel, float *ptr)
    {
        if (channel >= nChannels)
            return;
        vChannels[channel].vOut     = ptr;
    }

    void DamageDetector::generate_events(channel_t *c, size_t samples)
    {
        float s = 0.0f; // Current sample

        for (size_t i=0; i<samples; ++i)
        {
            s               = vBuffer[i];
            timestamp_t ts  = nTimestamp + i;

            // Update trigger state
            switch (c->enState)
            {
                case TRG_CLOSED:
                    if (s < fThreshold)
                        break;

                    c->enState      = TRG_OPENING;
                    c->nRaiseTime   = ts;
                    break;
                case TRG_OPENING:
                    if (s < fThreshold)
                        c->enState      = TRG_CLOSED;
                    else if ((ts - c->nRaiseTime) > nBounceTime)
                    {
                        c->nOpenTime    = ts;
                        c->enState      = TRG_OPEN;
                    }
                    break;

                case TRG_OPEN:
                    if (s >= fThreshold)
                        break;

                    c->enState      = TRG_CLOSING;
                    c->nFallTime    = ts;
                    break;

                case TRG_CLOSING:
                    if (s >= fThreshold)
                        c->enState  = TRG_OPEN;
                    else if ((ts - c->nFallTime) > nBounceTime)
                    {
                        c->nCloseTime   = ts;
                        c->enState      = TRG_CLOSED;

                        // We need to check that we have had enough time trigger was opened
                        if (c->nFallTime < (c->nRaiseTime + nDetectTime))
                        {
                            const size_t events = push_event(&c->sEvBuf, ts);
                            c->nEvents      = lsp::lsp_max(c->nEvents, events);
                        }

                        // Output the event detection signal
                        if (!bBypass)
                            c->vOut[i]      = 1.0f;
                    }
                    break;

                default:
                    break;
            }
        }
    }

    void DamageDetector::process(size_t samples)
    {
        // Apply new changes if they are
        update_settings();

        // Prepare data
        for (size_t i=0; i<nChannels; ++i)
        {
            channel_t *c    = &vChannels[i];
            c->nEvents      = c->sEvBuf.nCount;
        }

        // Pass data from input to output
        for (size_t offset = 0; offset < samples; )
        {
            const size_t to_do = lsp::lsp_min(samples - offset, TMP_BUFFER_SIZE);

            // Process each channel
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Process sidechain and apply bypass
                lsp::dsp::sanitize2(c->vOut, c->vIn, to_do);
                c->sSC.process(vBuffer, const_cast<const float **>(&c->vOut), to_do);
                if (!bBypass)
                    lsp::dsp::fill_zero(c->vOut, samples);

                // Generate events triggered by the detector
                generate_events(c, to_do);

                // Update pointers
                c->vIn     += to_do;
                c->vOut    += to_do;
            }

            offset     += to_do;
            nTimestamp += to_do;
        }

        // Cleanup state
        for (size_t i=0; i<nChannels; ++i)
        {
            channel_t *c    = &vChannels[i];

            // Remove old events from buffer
            update_event_buf(&c->sEvBuf, nTimestamp);

            c->vIn          = NULL;
            c->vOut         = NULL;
        }
    }

    size_t DamageDetector::events_count(size_t channel) const
    {
        return (channel < nChannels) ? vChannels[channel].nEvents : 0;
    }

    size_t DamageDetector::events_count() const
    {
        size_t result = 0;
        for (size_t i=0; i<nChannels; ++i)
            result     += vChannels[i].nEvents;

        return result;
    }

} /* namespace dd */


