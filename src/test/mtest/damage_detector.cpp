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


#include <lsp-plug.in/test-fw/mtest.h>
#include <lsp-plug.in/dsp-units/sampling/Sample.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/dsp-units/util/Sidechain.h>

#include <private/DamageDetector.h>

MTEST_BEGIN("damage_detector", damage_detector)

    static constexpr size_t BLOCK_SIZE  = 0x200;

    void process_dd(dspu::Sample *out, dspu::Sample *count, const dspu::Sample *src)
    {
        dspu::Sample o_tmp, c_tmp;

        o_tmp.init(src->channels(), src->length());
        o_tmp.set_sample_rate(src->sample_rate());

        c_tmp.init(1, src->length());
        c_tmp.set_sample_rate(src->sample_rate());

        // Process data with blocks
        dd::DamageDetector detector(2);
        detector.set_bypass(false);
        detector.set_sample_rate(src->sample_rate());
        detector.set_threshold(-60.0f);
        detector.set_event_period(2.0f);
        detector.set_event_threshold(18);

        for (size_t offset=0; offset < src->length(); )
        {
            const size_t to_do = lsp::lsp_min(BLOCK_SIZE, src->length());

            detector.bind_input(0, src->channel(0, offset));
            detector.bind_input(1, src->channel(1, offset));
            detector.bind_output(0, o_tmp.channel(0, offset));
            detector.bind_output(1, o_tmp.channel(1, offset));

            detector.process(to_do);

            float *dptr = c_tmp.channel(0, offset);

            size_t events = detector.events_count();
            if (events > 0)
                printf("offset = %d, events = %d\n", int(offset), int(events));
            dsp::fill(dptr, events * 0.01f, to_do);

            dd::event_type_t ev = detector.poll_event();
            if (ev != dd::EVENT_NONE)
            {
                printf("offset = %d, event = %s\n", int(offset), (ev == dd::EVENT_ABOVE) ? "ABOVE" : "BELOW");
                *dptr = (ev == dd::EVENT_ABOVE) ? 1.0f : -1.0f;
            }

            offset     += to_do;
        }

        o_tmp.swap(out);
        c_tmp.swap(count);
    }

    MTEST_MAIN
    {
        dspu::Sample in, out, count;
        io::Path path;

        MTEST_ASSERT(path.fmt("%s/input.wav", resources()) > 0);
        MTEST_ASSERT(in.load(&path) == STATUS_OK);

        process_dd(&out, &count, &in);

        MTEST_ASSERT(path.fmt("%s/%s-detect.wav", tempdir(), full_name()) > 0);
        MTEST_ASSERT(out.save(&path) > 0);
        printf("Saved result to file %s\n", path.as_native());

        MTEST_ASSERT(path.fmt("%s/%s-count.wav", tempdir(), full_name()) > 0);
        MTEST_ASSERT(count.save(&path) > 0);
        printf("Saved result to file %s\n", path.as_native());
    }

MTEST_END

