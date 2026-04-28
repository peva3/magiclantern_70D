/*
 * Copyright (C) 2025 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _mlv_3_h_
#define _mlv_3_h_

// This header contains all that is required for external code to
// create MLV files.  Things that use the MLV 3 library don't need to
// know anything about the structure of MLV files.  Conceptually, they
// are just storing image data.
//
// No ML internal headers (raw.h, lens.h, fps.h, propvalues.h) are
// included here.  Callers populate struct mlv_session with snapshots
// of the data needed for MLV metadata blocks.

// Local raw_info struct matching raw.h struct raw_info layout (32-bit ARM).
// This avoids a dependency on raw.h for the MLV library.  Caller must
// populate this from raw_info before starting a session.
struct mlv_raw_info {
    uint32_t api_version;
    void* buffer;
    int32_t height, width, pitch;
    int32_t frame_size;
    int32_t bits_per_pixel;
    int32_t black_level;
    int32_t white_level;
    struct { int32_t x, y, width, height; } jpeg;
    struct { int32_t y1, x1, y2, x2; } active_area;
    int32_t exposure_bias[2];
    int32_t cfa_pattern;
    int32_t calibration_illuminant1;
    int32_t color_matrix1[18];
    int32_t dynamic_range;
};

struct mlv_session
{
    int fps;
    int res_x;
    int res_y;
    int crop_offset_x;
    int crop_offset_y;
    int pan_offset_x;
    int pan_offset_y;
    uint32_t image_data_alignment;

    // RAWI block data (formerly from raw_info global)
    struct mlv_raw_info raw_info;
    int bpp;

    // RAWC block data (formerly from raw_capture_info global)
    uint16_t sensor_res_x;
    uint16_t sensor_res_y;
    uint16_t sensor_crop;
    uint8_t  binning_x;
    uint8_t  skipping_x;
    uint8_t  binning_y;
    uint8_t  skipping_y;
    int16_t  offset_x;
    int16_t  offset_y;

    // EXPO block data (formerly from lens_info + fps globals)
    uint32_t iso;
    uint32_t iso_auto;
    uint32_t iso_analog_raw;
    int      iso_digital_ev;
    uint32_t shutter_reciprocal;

    // LENS block data (formerly from lens_info + af_mode globals)
    uint16_t focal_length;
    uint16_t focus_dist;
    uint16_t aperture;
    uint8_t  stabilizer_mode;
    uint8_t  autofocus_mode;
    uint32_t lens_ID;
    char     lens_name[32];
    char     lens_serial[32];

    // WBAL block data (formerly from lens_info global)
    uint32_t wb_mode;
    uint32_t kelvin;
    uint32_t wbgain_r;
    uint32_t wbgain_g;
    uint32_t wbgain_b;
    int8_t   wbs_gm;
    int8_t   wbs_ba;

    // IDNT block data (formerly from camera_model_id, camera_model, camera_serial globals)
    uint32_t camera_model;
    char     camera_name[32];
    char     camera_serial[32];
};

int start_mlv_session(struct mlv_session *session);
int stop_mlv_session(void);

int write_file_headers(char *start, char *end);
char *write_video_frame_header(uint32_t image_size, char *buf_start, char *buf_end);
int update_video_frame_count(uint32_t count, char *start, char *end);

#endif
