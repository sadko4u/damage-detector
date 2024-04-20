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

#ifndef PRIVATE_DAMAGEDETECTOR_H_
#define PRIVATE_DAMAGEDETECTOR_H_

#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/dsp-units/util/Sidechain.h>

namespace dd
{
    enum event_type_t
    {
        EVENT_NONE,     // No event
        EVENT_ABOVE,    // The number of stream corruptions is above the threshold
        EVENT_BELOW     // The number of stream corruptions is below the threshold
    };

    typedef uint64_t            timestamp_t;

    class DamageDetector
    {
        public:
            static constexpr size_t MAX_EVENTS          = 0x1000;

            static constexpr float  MIN_REACTIVITY      = 0.1f;
            static constexpr float  MAX_REACTIVITY      = 20.0f;
            static constexpr float  DFL_REACTIVITY      = 10.0f;

            static constexpr float  MIN_DETECT_TIME     = 0.50f;
            static constexpr float  MAX_DETECT_TIME     = 5.0f;
            static constexpr float  DFL_DETECT_TIME     = 1.0f;

            static constexpr float  MIN_ESTIMATE_TIME   = 1.0f;
            static constexpr float  MAX_ESTIMATE_TIME   = 60.0f;
            static constexpr float  DFL_ESTIMATE_TIME   = 10.0f;

            static constexpr float  MIN_THRESHOLD       = -100.0f;
            static constexpr float  MAX_THRESHOLD       = 0.0f;
            static constexpr float  DFL_THRESHOLD       = -40.0f;

            static constexpr float  MIN_EV_PERIOD       = 0.01f;
            static constexpr float  MAX_EV_PERIOD       = 60.0f;
            static constexpr float  DFL_EV_PERIOD       = 1.0f;

            static constexpr size_t DFL_EV_TRHESHOLD    = 10;

        private:
            enum trg_state_t
            {
                TRG_CLOSED,
                TRG_OPENING,
                TRG_OPEN,
                TRG_CLOSING
            };

            typedef struct event_t
            {
                timestamp_t             nTimestamp;
            } event_t;

            typedef struct event_buf_t
            {
                event_t    *vData;
                uint32_t    nHead;
                uint32_t    nTail;
                uint32_t    nCount;
            } event_buf_t;

            typedef struct channel_t
            {
                lsp::dspu::Sidechain    sSC;
                event_buf_t             sEvBuf;
                timestamp_t             nOpenTime;
                timestamp_t             nCloseTime;
                timestamp_t             nRaiseTime;     // Last time the signal went above threshold
                timestamp_t             nFallTime;      // Last time the signal went below threshold
                uint32_t                nEvents;        // Number of computed events
                trg_state_t             enState;        // State of the trigger

                const float            *vIn;            // Input buffer
                float                  *vOut;           // Output buffer
            } channel_t;

        private:
            channel_t      *vChannels;      // Audio channels
            float          *vBuffer;        // Temporary buffer for processing
            timestamp_t     nTimestamp;     // Audio processing timestamp
            timestamp_t     nLastNotify;    // Last notification time
            uint32_t        nChannels;      // Number of channels
            uint32_t        nSampleRate;    // Sample rate
            uint32_t        nDetectTime;    // Detection time in samples
            uint32_t        nBounceTime;    // Raise/Fall detection time
            uint32_t        nEstimateTime;  // Overall estimation time
            uint32_t        nEventPeriod;   // Event period
            uint32_t        nEventThreshold;// Event threshold
            float           fDetectTime;    // Detection time in milliseconds
            float           fThresholdDB;   // Threshold (in decibels)
            float           fThreshold;     // Threshold
            float           fReactivity;    // Reactivity
            float           fEstimateTime;  // Estimation time
            float           fEventPeriod;   // Event period
            event_type_t    enLastEvent;    // Last delivered event
            event_type_t    enPendingEvent; // Pending event
            bool            bBypass;        // Bypass
            bool            bUpdate;        // Update data

            uint8_t        *pData;

        public:
            DamageDetector(size_t channels);
            DamageDetector(const DamageDetector &) = delete;
            DamageDetector(DamageDetector &&) = delete;
            ~DamageDetector();

            DamageDetector & operator = (const DamageDetector &) = delete;
            DamageDetector & operator = (DamageDetector &&) = delete;

        private:
            void            update_settings();
            void            generate_events(channel_t *c, size_t to_do);
            size_t          push_event(event_buf_t *buf, timestamp_t ts);
            void            update_event_buf(event_buf_t *buf, timestamp_t ts);

        private:
            static void     clear_event_buf(event_buf_t *buf);

        public:
            /**
             * Get current timestamp in samples
             * @return current timestamp in samples
             */
            inline timestamp_t timestamp() const        { return nTimestamp; }

            /**
             * Set processing sample rate
             * @param sample_rate processing sample rate
             */
            void            set_sample_rate(size_t sample_rate);
            inline size_t   sample_rate() const { return nSampleRate; }

            /**
             * Set audio click detection time in seconds
             * @param detect_time audio click detection time
             */
            void            set_detect_time(float detect_time);
            inline float    detect_time() const { return fDetectTime; }

            /**
             * Set the estimation time window for calculating number of events in seconds
             * @param est_time estimation time window in seconds
             */
            void            set_estimation_time(float est_time);
            inline float    estimation_time() const { return nEstimateTime; }

            /**
             * Set trigger threshold
             * @param thresh trigger threshold in decibels
             */
            void            set_threshold(float thresh);
            inline float    threshold() const { return fThresholdDB; }

            /**
             * Enable/disable bypass
             * @param bypass bypass flag
             */
            void            set_bypass(bool bypass);
            inline bool     bypass() const { return bBypass; }

            /**
             * Set the reactivity of the RMS value calculation in milliseconds
             * @param reactivity reactivity of the RMS value calculation
             */
            void            set_reactivity(float reactivity);
            inline float    reactivity() const { return fReactivity; }

            /**
             * Set stream corruption event shipping period in seconds
             * @param period period
             */
            void            set_event_period(float period);
            inline float    event_period() const { return fEventPeriod; }

            /**
             * Set number of events that allow to consider the stream being corrupted
             * @param threshold number of events that allow to consider the stream being corrupted
             */
            void            set_event_threshold(size_t threshold);
            inline float    event_threshold() const { return nEventThreshold; }\

            /**
             * Poll current pending event and cleanup
             * @return the pending event
             */
            event_type_t    poll_event();

            /**
             * Bind input buffer
             * @param channel audio channel index
             * @param ptr pointer to the channel data
             */
            void            bind_input(size_t channel, const float *ptr);

            /**
             * Bind output buffer
             * @param channel audio channel index
             * @param ptr pointer to the channel data
             */
            void            bind_output(size_t channel, float *ptr);

            /**
             * Process audio data
             * @param samples number of samples to process
             */
            void            process(size_t samples);

            /**
             * Return number of events detected for the audio channel
             * @param channel audio channel index
             * @return number of events detected for the audio channel
             */
            size_t          events_count(size_t channel) const;

            /**
             * Return number of events detected for all audio channels
             * @param channel audio channel index
             * @return number of events detected for all audio channels
             */
            size_t          events_count() const;
    };

} /* namespace dd */


#endif /* PRIVATE_DAMAGEDETECTOR_H_ */
