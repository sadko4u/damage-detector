/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of damage-detector
 * Created on: 16 апр. 2024 г.
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

#ifndef PRIVATE_VERSION_H_
#define PRIVATE_VERSION_H_

#define DAMAGE_DETECTOR_MAJOR           1
#define DAMAGE_DETECTOR_MINOR           0
#define DAMAGE_DETECTOR_MICRO           0

#define DAMAGE_DETECTOR_PACKAGE         "damage-detector"
#define DAMAGE_DETECTOR_LICENSE         "LGPL"
#define DAMAGE_DETECTOR_ORIGIN          "https://github.com/sadko4u/damage-detector"

#if defined(DAMAGE_DETECTOR_PUBLISHER)
    #define DAMAGE_DETECTOR_PUBLIC      LSP_EXPORT_MODIFIER
#elif defined(DAMAGE_DETECTOR_BUILTIN) || defined(LSP_IDE_DEBUG)
    #define DAMAGE_DETECTOR_PUBLIC
#else
    #define DAMAGE_DETECTOR_PUBLIC      LSP_SYMBOL_EXTERN
#endif

#endif /* PRIVATE_VERSION_H_ */
