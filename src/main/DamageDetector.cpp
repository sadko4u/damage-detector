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
        fDetectTime                 = DFL_DETECT_TIME;
        fThreshold                  = DFL_THRESHOLD;
        fReactivity                 = DFL_REACTIVITY;
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

        for (size_t i=0; i<channels; ++channels)
        {
            channel_t *c                = &vChannels[i];

            c->sSC.construct();
            c->sSC.init(1, MAX_REACTIVITY);
            c->sSC.set_mode(lsp::dspu::SCM_RMS);
            c->sSC.set_source(lsp::dspu::SCS_MIDDLE);

            c->sEvBuf.nHead             = 0.0f;
            c->sEvBuf.nTail             = 0.0f;
            c->sEvBuf.nCount            = 0.0f;

            c->nRaiseTime               = 0;
            c->nFallTime                = 0;

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

    void DamageDetector::update_settings()
    {
        if (!bUpdate)
            return;

        nDetectTime = lsp::dspu::seconds_to_samples(nSampleRate, fDetectTime);

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
        if (nDetectTime == detect_time)
            return;
        nDetectTime     = detect_time;
        bUpdate         = false;
    }

    void DamageDetector::set_bypass(bool bypass)
    {
        bBypass         = bypass;
    }

    void DamageDetector::set_threshold(float thresh)
    {
        thresh          = lsp::lsp_limit(thresh, MIN_THRESHOLD, MAX_THRESHOLD);
        fThreshold      = lsp::dspu::db_to_gain(thresh);
    }

    void DamageDetector::bind_input(size_t channel, const float *ptr)
    {
        if (channel < nChannels)
            return;
        vChannels[channel].vIn      = ptr;
    }

    void DamageDetector::bind_output(size_t channel, float *ptr)
    {
        if (channel < nChannels)
            return;
        vChannels[channel].vOut     = ptr;
    }

    void DamageDetector::process(size_t samples)
    {
        // Apply new changes if they are
        update_settings();

        // Do the rest stuff
        for (size_t i=0; i<nChannels; ++i)
        {
            channel_t *c = &vChannels[i];

            // Pass data from input to output
            for (size_t offset = 0; offset < samples; )
            {
                const size_t to_do = lsp::lsp_min(samples - offset, TMP_BUFFER_SIZE);

                // Process sidechain and apply bypass
                lsp::dsp::sanitize2(c->vOut, c->vIn, to_do);
                c->sSC.process(vBuffer, const_cast<const float **>(&c->vIn), to_do);
                if (!bBypass)
                    lsp::dsp::fill_zero(c->vOut, samples);

                // TODO: main detection stuff

                // Update pointers
                c->vIn     += to_do;
                c->vOut    += to_do;
                offset     += to_do;
            }
        }
    }

    size_t DamageDetector::events_count(size_t channel) const
    {
        return (channel < nChannels) ? vChannels[channel].sEvBuf.nCount : 0;
    }

    size_t DamageDetector::events_count() const
    {
        size_t result = 0;
        for (size_t i=0; i<nChannels; ++i)
            result     += vChannels[i].sEvBuf.nCount;

        return result;
    }

} /* namespace dd */


