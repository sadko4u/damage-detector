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

MTEST_BEGIN("damage_detector", rms_detect)

    static constexpr float RMS_TIME_MS = 20.0f;


    void process_rms(dspu::Sample *dst, const dspu::Sample *src)
    {
        dspu::Sample tmp;
        const size_t rms_tail = dspu::millis_to_samples(src->sample_rate(), RMS_TIME_MS);

        tmp.copy(src);
        tmp.append(rms_tail);

        dspu::Sidechain sc;
        sc.init(1, RMS_TIME_MS);
        sc.set_gain(1.0f);
        sc.set_mode(dspu::SCM_RMS);
        sc.set_reactivity(RMS_TIME_MS);
        sc.set_sample_rate(src->sample_rate());
        sc.set_source(dspu::SCS_MIDDLE);

        for (size_t i=0; i<src->channels(); ++i)
        {
            float *ptr = tmp.channel(i);

            sc.process(ptr, const_cast<const float **>(&ptr), tmp.length());
            sc.clear();
        }

        tmp.remove(0, rms_tail/2);
        tmp.set_length(src->length());
        tmp.swap(dst);
    }

    MTEST_MAIN
    {
        dspu::Sample in, out;
        io::Path path;

        MTEST_ASSERT(path.fmt("%s/input.wav", resources()) > 0);
        MTEST_ASSERT(in.load(&path) == STATUS_OK);

        process_rms(&out, &in);

        MTEST_ASSERT(path.fmt("%s/%s-rms.wav", tempdir(), full_name()) > 0);
        MTEST_ASSERT(out.save(&path) > 0);

        printf("Saved result to file %s\n", path.as_native());
    }

MTEST_END

