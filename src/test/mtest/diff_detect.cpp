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

MTEST_BEGIN("damage_detector", diff_detect)

    void process_diff(dspu::Sample *dst, const dspu::Sample *src)
    {
        dspu::Sample tmp;

        tmp.copy(src);

        for (size_t i=0; i<src->channels(); ++i)
        {
            float *ptr = tmp.channel(i);
            float prev = 0.0f;

            for (size_t j=0; j<src->length(); ++j)
            {
                const float s   = ptr[j];
                ptr[j]          = s - prev;
                prev            = s;
            }
        }

        tmp.swap(dst);
    }

    MTEST_MAIN
    {
        dspu::Sample in, diff, diff2, out;
        io::Path path;

        MTEST_ASSERT(path.fmt("%s/input.wav", resources()) > 0);
        MTEST_ASSERT(in.load(&path) == STATUS_OK);

        process_diff(&diff, &in);
        process_diff(&diff2, &diff);
        process_diff(&out, &diff2);

        MTEST_ASSERT(path.fmt("%s/%s-diff3.wav", tempdir(), full_name()) > 0);
        MTEST_ASSERT(out.save(&path) > 0);

        printf("Saved result to file %s\n", path.as_native());
    }

MTEST_END

