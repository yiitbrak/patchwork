/*
 * pw-enums.h
 *
 * Copyright 2023 Yiğit <>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

typedef enum {
  PW_PAD_DIRECTION_INVALID,
  PW_PAD_DIRECTION_OUT,
  PW_PAD_DIRECTION_IN,
}PwPadDirection;

typedef enum {
  PW_PAD_TYPE_AUDIO,
  PW_PAD_TYPE_VIDEO,
  PW_PAD_TYPE_MIDI,
  PW_PAD_TYPE_MIDI_PASSTHROUGH,
  PW_PAD_TYPE_OTHER
}PwPadType;
