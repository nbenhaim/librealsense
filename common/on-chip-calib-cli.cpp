// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <glad/glad.h>
#include "on-chip-calib-cli.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <model-views.h>
#include <viewer.h>
#include <ds5/ds5-private.h>
#include <types.h>


#include "../tools/depth-quality/depth-metrics.h"

namespace rs2
{
#pragma pack(push, 1)
#pragma pack(1)
    struct table_header
    {
        uint16_t version;        // major.minor. Big-endian
        uint16_t table_type;     // ctCalibration
        uint32_t table_size;     // full size including: TOC header + TOC + actual tables
        uint32_t param;          // This field content is defined ny table type
        uint32_t crc32;          // crc of all the actual table data excluding header/CRC
    };

    enum rs2_dsc_status : uint16_t
    {
        RS2_DSC_STATUS_SUCCESS = 0, /**< Self calibration succeeded*/
        RS2_DSC_STATUS_RESULT_NOT_READY = 1, /**< Self calibration result is not ready yet*/
        RS2_DSC_STATUS_FILL_FACTOR_TOO_LOW = 2, /**< There are too little textures in the scene*/
        RS2_DSC_STATUS_EDGE_TOO_CLOSE = 3, /**< Self calibration range is too small*/
        RS2_DSC_STATUS_NOT_CONVERGE = 4, /**< For tare calibration only*/
        RS2_DSC_STATUS_BURN_SUCCESS = 5,
        RS2_DSC_STATUS_BURN_ERROR = 6,
        RS2_DSC_STATUS_NO_DEPTH_AVERAGE = 7,
    };

#define MAX_STEP_COUNT 256

    struct DirectSearchCalibrationResult
    {
        uint32_t opcode;
        uint16_t status;      // DscStatus
        uint16_t stepCount;
        uint16_t stepSize; // 1/1000 of a pixel
        uint32_t pixelCountThreshold; // minimum number of pixels in
                                      // selected bin
        uint16_t minDepth;  // Depth range for FWHM
        uint16_t maxDepth;
        uint32_t rightPy;   // 1/1000000 of normalized unit
        float healthCheck;
        float rightRotation[9]; // Right rotation
        //uint16_t results[0]; // 1/100 of a percent
    };

    typedef struct
    {
        uint16_t m_status;
        float    m_healthCheck;
    } DscResultParams;

    typedef struct
    {
        uint16_t m_paramSize;
        DscResultParams m_dscResultParams;
        uint16_t m_tableSize;
    } DscResultBuffer;
#pragma pack(pop)

    void on_chip_calib_cli_manager::stop_viewer(invoker invoke)
    {
        int x;
        // try
        // {
        //     auto profiles = _sub->get_selected_profiles();

        //     invoke([&](){
        //         // Stop viewer UI
        //         _sub->stop(_viewer);
        //     });

        //     // Wait until frames from all active profiles stop arriving
        //     bool frame_arrived = false;
        //     while (frame_arrived && _viewer.streams.size())
        //     {
        //         for (auto&& stream : _viewer.streams)
        //         {
        //             if (std::find(profiles.begin(), profiles.end(),
        //                 stream.second.original_profile) != profiles.end())
        //             {
        //                 auto now = std::chrono::high_resolution_clock::now();
        //                 if (now - stream.second.last_frame > std::chrono::milliseconds(200))
        //                     frame_arrived = false;
        //             }
        //             else frame_arrived = false;
        //         }
        //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //     }
        // }
        // catch (...) {}
    }

    // Wait for next depth frame and return it
    rs2::depth_frame on_chip_calib_cli_manager::fetch_depth_frame(rs2::pipeline & pipe)
    {

        // printf("****waiting for frames\n");
        rs2::frameset frames = pipe.wait_for_frames();
        // printf("got frames\n");

        // printf("getting depth frame\n");
        // // Try to get a frame of a depth image
        rs2::depth_frame f = frames.get_depth_frame();

        return f;
        // auto profiles = _sub->get_selected_profiles();
        // bool frame_arrived = false;
        // rs2::depth_frame res = rs2::frame{};
        // while (!frame_arrived)
        // {
        //     for (auto&& stream : _viewer.streams)
        //     {
        //         if (std::find(profiles.begin(), profiles.end(),
        //             stream.second.original_profile) != profiles.end())
        //         {
        //             auto now = std::chrono::high_resolution_clock::now();
        //             if (now - stream.second.last_frame < std::chrono::milliseconds(100))
        //             {
        //                 if (auto f = stream.second.texture->get_last_frame(false).as<rs2::depth_frame>())
        //                 {
        //                     frame_arrived = true;
        //                     res = f;
        //                 }
        //             }
        //         }
        //     }
        //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // }
        // return res;
    }

    void on_chip_calib_cli_manager::start_viewer(int w, int h, int fps, invoker invoke)
    {
        // try
        // {
        //     if (_ui) _sub->ui = *_ui; // Save previous configuration

        //     // Select only depth stream
        //     _sub->ui.selected_format_id.clear();
        //     _sub->ui.selected_format_id[RS2_STREAM_DEPTH] = 0;

        //     // Select FPS value
        //     for (int i = 0; i < _sub->shared_fps_values.size(); i++)
        //     {
        //         if (_sub->shared_fps_values[i] == fps)
        //             _sub->ui.selected_shared_fps_id = i;
        //     }

        //     // Select Resolution
        //     for (int i = 0; i < _sub->res_values.size(); i++)
        //     {
        //         auto kvp = _sub->res_values[i];
        //         if (kvp.first == w && kvp.second == h)
        //             _sub->ui.selected_res_id = i;
        //     }

        //     // If not supported, try WxHx30
        //     if (!_sub->is_selected_combination_supported())
        //     {
        //         for (int i = 0; i < _sub->shared_fps_values.size(); i++)
        //         {
        //             if (_sub->shared_fps_values[i] == 30)
        //                 _sub->ui.selected_shared_fps_id = i;
        //         }

        //         // If still not supported, try VGA30
        //         if (!_sub->is_selected_combination_supported())
        //         {
        //             for (int i = 0; i < _sub->res_values.size(); i++)
        //             {
        //                 auto kvp = _sub->res_values[i];
        //                 if (kvp.first == 640 && kvp.second == 480)
        //                     _sub->ui.selected_res_id = i;
        //             }
        //         }
        //     }

        //     auto profiles = _sub->get_selected_profiles();

        //     invoke([&](){
        //         if (!_model.dev_syncer)
        //             _model.dev_syncer = _viewer.syncer->create_syncer();

        //         // Start streaming
        //         _sub->play(profiles, _viewer, _model.dev_syncer);
        //         for (auto&& profile : profiles)
        //         {
        //             _viewer.begin_stream(_sub, profile);
        //         }
        //     });

        //     // Wait for frames to arrive
        //     bool frame_arrived = false;
        //     int count = 0;
        //     while (!frame_arrived && count++ < 100)
        //     {
        //         for (auto&& stream : _viewer.streams)
        //         {
        //             if (std::find(profiles.begin(), profiles.end(),
        //                 stream.second.original_profile) != profiles.end())
        //             {
        //                 auto now = std::chrono::high_resolution_clock::now();
        //                 if (now - stream.second.last_frame < std::chrono::milliseconds(100))
        //                     frame_arrived = true;
        //             }
        //         }
        //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //     }
        // }
        // catch (...) {}
    }



    std::pair<float, float> on_chip_calib_cli_manager::get_metric(bool use_new)
    {
        return _metrics[use_new ? 1 : 0];
    }

    std::pair<float, float> on_chip_calib_cli_manager::get_depth_metrics(rs2::pipeline & pipe)
    {
        // using namespace depth_quality;

        auto f = fetch_depth_frame(pipe);
        printf("got depth frame\n");
        auto sensor = std::make_shared<rs2::sensor>(_dev.first<depth_stereo_sensor>());
        
        // auto sensor = _sub->s->as<rs2::depth_stereo_sensor>();
        auto intr = f.get_profile().as<rs2::video_stream_profile>().get_intrinsics();
        rs2::region_of_interest roi { (int)(f.get_width() * 0.45f), (int)(f.get_height()  * 0.45f), 
                                       (int)(f.get_width() * 0.55f), (int)(f.get_height() * 0.55f) };
         
        std::vector<depth_quality::single_metric_data> v;

        std::vector<float> fill_rates;
        std::vector<float> rmses;

        float depth_scale = _dev.first<depth_stereo_sensor>().get_depth_scale();
        float baseline =  _dev.first<depth_stereo_sensor>().get_stereo_baseline();

        // auto show_plane = _viewer.draw_plane;

        auto on_frame = [sensor, &fill_rates, &rmses, this](
            const std::vector<rs2::float3>& points,
            const plane p,
            const rs2::region_of_interest roi,
            const float baseline_mm,
            const float focal_length_pixels,
            const int ground_thruth_mm,
            const bool plane_fit,
            const float plane_fit_to_ground_truth_mm,
            const float distance_mm,
            bool record,
            std::vector<depth_quality::single_metric_data>& samples)
        {
            float TO_METERS = _dev.first<depth_sensor>().get_depth_scale();

            static const float TO_MM = 1000.f;
            static const float TO_PERCENT = 100.f;

            // Calculate fill rate relative to the ROI
            auto fill_rate = points.size() / float((roi.max_x - roi.min_x)*(roi.max_y - roi.min_y)) * TO_PERCENT;
            fill_rates.push_back(fill_rate);

            if (!plane_fit) return;

            std::vector<rs2::float3> points_set = points;
            std::vector<float> distances;

            // Reserve memory for the data
            distances.reserve(points.size());

            // Convert Z values into Depth values by aligning the Fitted plane with the Ground Truth (GT) plane
            // Calculate distance and disparity of Z values to the fitted plane.
            // Use the rotated plane fit to calculate GT errors
            for (auto point : points_set)
            {
                // Find distance from point to the reconstructed plane
                auto dist2plane = p.a*point.x + p.b*point.y + p.c*point.z + p.d;
                // Project the point to plane in 3D and find distance to the intersection point
                rs2::float3 plane_intersect = { float(point.x - dist2plane*p.a),
                    float(point.y - dist2plane*p.b),
                    float(point.z - dist2plane*p.c) };

                // Store distance, disparity and gt- error
                distances.push_back(dist2plane * TO_MM);
            }

            // Remove outliers [below 1% and above 99%)
            std::sort(points_set.begin(), points_set.end(), [](const rs2::float3& a, const rs2::float3& b) { return a.z < b.z; });
            size_t outliers = points_set.size() / 50;
            points_set.erase(points_set.begin(), points_set.begin() + outliers); // crop min 0.5% of the dataset
            points_set.resize(points_set.size() - outliers); // crop max 0.5% of the dataset

            // Calculate Plane Fit RMS  (Spatial Noise) mm
            double plane_fit_err_sqr_sum = std::inner_product(distances.begin(), distances.end(), distances.begin(), 0.);
            auto rms_error_val = static_cast<float>(std::sqrt(plane_fit_err_sqr_sum / distances.size()));
            auto rms_error_val_per = TO_PERCENT * (rms_error_val / distance_mm);
            rmses.push_back(rms_error_val_per);
        };

        auto rms_std = 1000.f;
        auto new_rms_std = rms_std;
        auto count = 0;

        // Capture metrics on bundles of 31 frame
        // Repeat until get "decent" bundle or reach 10 sec
        do
        {
            rms_std = new_rms_std;

            rmses.clear();
            
            for (int i = 0; i < 31; i++)
            {
                f = fetch_depth_frame(pipe);
                auto res = depth_quality::analyze_depth_image(f, depth_scale, baseline,
                    &intr, roi, 0, true, v, false, on_frame);
              
            }

            auto rmses_sum_sqr = std::inner_product(rmses.begin(), rmses.end(), rmses.begin(), 0.);
            new_rms_std = static_cast<float>(std::sqrt(rmses_sum_sqr / rmses.size()));
        } while ((new_rms_std < rms_std * 0.8f && new_rms_std > 10.f) && count++ < 10);

        std::sort(fill_rates.begin(), fill_rates.end());
        std::sort(rmses.begin(), rmses.end());

        float median_fill_rate, median_rms;
        if (fill_rates.empty())
            median_fill_rate = 0;
        else
            median_fill_rate = fill_rates[fill_rates.size() / 2];
        if (rmses.empty())
            median_rms = 0;
        else
            median_rms = rmses[rmses.size() / 2];

     
        return { median_fill_rate, median_rms };
       // return {};
    }

    std::vector<uint8_t> on_chip_calib_cli_manager::safe_send_command(
        const std::vector<uint8_t>& cmd, const std::string& name)
    {
        auto dp = _dev.as<debug_protocol>();
        if (!dp) throw std::runtime_error("Device does not support debug protocol!");

        auto res = dp.send_and_receive_raw_data(cmd);

        if (res.size() < sizeof(int32_t)) throw std::runtime_error(to_string() << "Not enough data from " << name << "!");
        auto return_code = *((int32_t*)res.data());
        if (return_code < 0)  throw std::runtime_error(to_string() << "Firmware error (" << return_code << ") from " << name << "!");

        return res;
    }

    void on_chip_calib_cli_manager::update_last_used()
    {
        // time_t rawtime;
        // time(&rawtime);
        // std::string id = to_string() << configurations::viewer::last_calib_notice << "." << _sub->s->get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
        // config_file::instance().set(id.c_str(), (long long)rawtime);
    }

    void on_chip_calib_cli_manager::calibrate()
    {
        rs2_dsc_status status = RS2_DSC_STATUS_BURN_SUCCESS;

        DirectSearchCalibrationResult result;

        if (tare)
        {
            std::vector<uint8_t> cmd =
            {
                0x14, 0x00, 0xab, 0xcd,
                0x80, 0x00, 0x00, 0x00,
                0x0b, 0x00, 0x00, 0x00,
                0xb8, 0x0b, 0x00, 0x00,
                0x1e, 0x1e, 0x03, 0x00,
                0x00, 0x00, 0x00, 0x00
            };
            uint32_t* param2 = (uint32_t*)cmd.data() + 3;
            *param2 = ground_truth * 100;
            cmd.data()[16] = average_step_count;
            cmd.data()[17] = step_count;
            cmd.data()[18] = accuracy;

            safe_send_command(cmd, "START_TARE");

            // While not ready...
            int count = 0;
            bool done = false;
            do
            {
                memset(&result, 0, sizeof(DirectSearchCalibrationResult));

                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                // Check calibration status
                cmd =
                {
                    0x14, 0x00, 0xab, 0xcd,
                    0x80, 0x00, 0x00, 0x00,
                    0x0c, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00
                };

                try
                {
                    auto res = safe_send_command(cmd, "CALIB_STATUS");

                    if (res.size() < sizeof(int32_t) + sizeof(DirectSearchCalibrationResult))
                        throw std::runtime_error("Not enough data from CALIB_STATUS!");

                    result = *reinterpret_cast<DirectSearchCalibrationResult*>(res.data());
                    done = result.status != RS2_DSC_STATUS_RESULT_NOT_READY;
                }
                catch (const std::exception& ex)
                {
                    //log(to_string() << "Warning: " << ex.what());
                }

                //_progress = count * (2 * speed);
            } while (count++ < 200 && !done);

            // If we exit due to counter, report timeout
            if (!done)
            {
                throw std::runtime_error("Operation timed-out!\n"
                    "Calibration state did not converged in time");
            }

            status = (rs2_dsc_status)result.status;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Begin auto-calibration
            std::vector<uint8_t> cmd =
            {
                0x14, 0x00, 0xab, 0xcd,
                0x80, 0x00, 0x00, 0x00,
                0x08, 0x00, 0x00, 0x00,
                (uint8_t)speed, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            };

            safe_send_command(cmd, "START_CALIB");

            memset(&result, 0, sizeof(DirectSearchCalibrationResult));

            // While not ready...
            int count = 0;
            bool done = false;
            do
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                // Check calibration status
                cmd =
                {
                    0x14, 0x00, 0xab, 0xcd,
                    0x80, 0x00, 0x00, 0x00,
                    0x03, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00
                };

                try
                {
                    auto res = safe_send_command(cmd, "CALIB_STATUS");

                    if (res.size() < sizeof(int32_t) + sizeof(DirectSearchCalibrationResult))
                        throw std::runtime_error("Not enough data from CALIB_STATUS!");

                    result = *reinterpret_cast<DirectSearchCalibrationResult*>(res.data());
                    done = result.status != RS2_DSC_STATUS_RESULT_NOT_READY;
                }
                catch (const std::exception& ex)
                {
                    //log(to_string() << "Warning: " << ex.what());
                }

                //_progress = count * (2 * speed);

            } while (count++ < 200 && !done);

            // If we exit due to counter, report timeout
            if (!done)
            {
                throw std::runtime_error("Operation timed-out!\n"
                    "Calibration state did not converged in time");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            status = (rs2_dsc_status)result.status;
        }

        // Handle errors from firmware
        if (status != RS2_DSC_STATUS_SUCCESS)
        {
            if (status == RS2_DSC_STATUS_EDGE_TOO_CLOSE)
            {
                throw std::runtime_error("Calibration didn't converge! (EDGE_TO_CLOSE)\n"
                    "Please retry in different lighting conditions");
            }
            else if (status == RS2_DSC_STATUS_FILL_FACTOR_TOO_LOW)
            {
                throw std::runtime_error("Not enough depth pixels! (FILL_FACTOR_LOW)\n"
                    "Please retry in different lighting conditions");
            }
            else if (status == RS2_DSC_STATUS_NOT_CONVERGE)
            {
                throw std::runtime_error("Calibration didn't converge! (NOT_CONVERGE)\n"
                    "Please retry in different lighting conditions");
            }
            else if (status == RS2_DSC_STATUS_NO_DEPTH_AVERAGE)
            {
                throw std::runtime_error("Calibration didn't converge! (NO_AVERAGE)\n"
                    "Please retry in different lighting conditions");
            }
            else throw std::runtime_error(to_string() << "Calibration didn't converge! (RESULT=" << result.status << ")");
        }
    }

    void on_chip_calib_cli_manager::stop_realsense()
    {
        _pipe.stop();
    }

    void on_chip_calib_cli_manager::start_realsense(int w, int h, int fps)
    {
        std::vector<rs2::config> cfgs;
        rs2::pipeline_profile active_profile;
      
        bool _device_in_use = false;

        {
            rs2::config cfg_default;
            // Preferred configuration Depth + Synthetic Color
            cfg_default.enable_stream(RS2_STREAM_DEPTH, -1, w, h, RS2_FORMAT_Z16, fps);
            cfg_default.enable_stream(RS2_STREAM_INFRARED, -1, w, h, RS2_FORMAT_RGB8, fps);
            cfgs.emplace_back(cfg_default);
        }
        // Use Infrared luminocity as a secondary video in case synthetic chroma is not supported
        {
            rs2::config cfg_alt;
            cfg_alt.enable_stream(RS2_STREAM_DEPTH, -1, w, h, RS2_FORMAT_Z16, fps);
            cfg_alt.enable_stream(RS2_STREAM_INFRARED, -1, w, h, RS2_FORMAT_Y8, fps);
            cfgs.emplace_back(cfg_alt);
        }

        bool valid_config = false;

        for (auto& cfg : cfgs)
        {
            if ((valid_config = cfg.can_resolve(_pipe)))
            {
                try {
                    printf("pipe started \n");
                    active_profile = _pipe.start(cfg);
                    valid_config = active_profile;
                    break;
                }
                catch (...)
                {
                    _pipe.stop();
                    valid_config = false;
                    if (!_device_in_use)
                    {
                        //window.add_on_load_message("Device is not functional or busy!");
                        printf("Device is not functional or busy!\n");
                        _device_in_use = true;
                    }
                }
            }
        }

        if (valid_config)
        {
            // Toggle advanced mode
            auto dev = _pipe.get_active_profile().get_device();
            if (dev.is<rs400::advanced_mode>())
            {
                auto advanced_mode = dev.as<rs400::advanced_mode>();
                if (!advanced_mode.is_enabled())
                {
                    // window.add_on_load_message("Toggling device into Advanced Mode...");
                    advanced_mode.toggle_advanced_mode(true);
                    valid_config = false;
                }
            }

            
        }

       

    }

    void on_chip_calib_cli_manager::save_calib(const char * filepath)
    {
        start_realsense(0,0,30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
       

        std::vector<uint8_t> current_calib;

        // Fetch current calibration using GETINITCAL command
        
        std::vector<uint8_t> fetch_calib{
            0x14, 0, 0xAB, 0xCD, 0x15, 0, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        
        auto calib = safe_send_command(fetch_calib, "GETINITCAL");
        if (calib.size() < sizeof(table_header) + sizeof(int32_t)) throw std::runtime_error("Missing calibration header from GETINITCAL!");

        auto table = (uint8_t*)(calib.data() + sizeof(int32_t) + sizeof(table_header));
        auto hd = (table_header*)(calib.data() + sizeof(int32_t));

        if (calib.size() < sizeof(table_header) + sizeof(int32_t) + hd->table_size) 
            throw std::runtime_error("Table truncated from GETINITCAL!");

        current_calib.resize(sizeof(table_header) + hd->table_size, 0);
        memcpy(current_calib.data(), hd, current_calib.size()); // Copy to old_calib

        std::ofstream calib_file;

        calib_file.open (filepath, std::ios::out | std::ios::binary);
        calib_file.write((const char *)current_calib.data(), current_calib.size());

        calib_file.close();

        stop_realsense();
    }

    void on_chip_calib_cli_manager::load_calib(const char * filepath)
    {
        start_realsense(0,0,30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
       

        std::vector<uint8_t> current_calib;

        // Fetch current calibration using GETINITCAL command
        
        std::vector<uint8_t> fetch_calib{
            0x14, 0, 0xAB, 0xCD, 0x15, 0, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        
        auto calib = safe_send_command(fetch_calib, "GETINITCAL");
        if (calib.size() < sizeof(table_header) + sizeof(int32_t)) throw std::runtime_error("Missing calibration header from GETINITCAL!");

        auto table = (uint8_t*)(calib.data() + sizeof(int32_t) + sizeof(table_header));
        auto hd = (table_header*)(calib.data() + sizeof(int32_t));

        if (calib.size() < sizeof(table_header) + sizeof(int32_t) + hd->table_size) 
            throw std::runtime_error("Table truncated from GETINITCAL!");

        current_calib.resize(sizeof(table_header) + hd->table_size, 0);
        memcpy(current_calib.data(), hd, current_calib.size()); // Copy to old_calib

        //now we have the current calibration data
        //we need to load the data from file into a vector
        std::ifstream calib_file;
        calib_file.open(filepath, std::ios::in | std::ios::binary);

        _new_calib.resize(sizeof(table_header) + hd->table_size, 0);
        calib_file.read((char*)_new_calib.data(), _new_calib.size());

        //size_t table_size = sizeof(table_header) + hd->table_size;
        size_t table_size = sizeof(librealsense::ds::coefficients_table);
        const librealsense::ds::coefficients_table * fromfile = librealsense::ds::check_calib<librealsense::ds::coefficients_table>(_new_calib);
        printf("loaded baseline is: %f\n", fromfile->baseline);

        librealsense::ds::coefficients_table copied_table;

        
        memcpy(&copied_table, fromfile, table_size);
        //copied_table.baseline = 55.0;
        
        memcpy(_new_calib.data(), &copied_table, table_size);

        //copied_table.header = librealsense::calc_crc32((const uint8_t *)&copied_table.header, sizeof(copied_table.header));
        copied_table.header.crc32 = librealsense::calc_crc32(_new_calib.data() + sizeof(table_header), _new_calib.size() - sizeof(table_header));

        memcpy(_new_calib.data(), &copied_table, table_size);

        printf("copied baseline is: %f\n", copied_table.baseline);

        
        apply_calib(true);
        keep();

        stop_realsense();
        printf("finished applying loaded calibration data\n");

        //std::ofstream calib_file;

        //calib_file.open (filepath, std::ios::out | std::ios::binary);
        //calib_file.write((const char *)current_calib.data(), current_calib.size());

        //calib_file.close();
    }

    void on_chip_calib_cli_manager::process_flow()
    {
    //     //update_last_used();
        start_realsense(0,0,30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        //log(to_string() << "Starting calibration at speed " << speed);

        //in_3d_view = _viewer.is_3d_view;
        //_viewer.is_3d_view = true;

        // Fetch current calibration using GETINITCAL command
        printf("1\n");
        std::vector<uint8_t> fetch_calib{
            0x14, 0, 0xAB, 0xCD, 0x15, 0, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        printf("2\n");
        auto calib = safe_send_command(fetch_calib, "GETINITCAL");
        if (calib.size() < sizeof(table_header) + sizeof(int32_t)) throw std::runtime_error("Missing calibration header from GETINITCAL!");

        printf("3\n");
        auto table = (uint8_t*)(calib.data() + sizeof(int32_t) + sizeof(table_header));
        auto hd = (table_header*)(calib.data() + sizeof(int32_t));

        printf("4\n");
        if (calib.size() < sizeof(table_header) + sizeof(int32_t) + hd->table_size) 
            throw std::runtime_error("Table truncated from GETINITCAL!");

        printf("5\n");
        _old_calib.resize(sizeof(table_header) + hd->table_size, 0);
        memcpy(_old_calib.data(), hd, _old_calib.size()); // Copy to old_calib

        //_was_streaming = _sub->streaming;
        //_synchronized = _viewer.synchronization_enable.load();
        //_post_processing = _sub->post_processing_enabled;
        //_sub->post_processing_enabled = false;
        //_viewer.synchronization_enable = false;

        _restored = false;

        //if (!_was_streaming) 
        //{
        //    start_viewer(0,0,0, invoke);
        //}

    //     // Capture metrics before


        auto metrics_before = get_depth_metrics(_pipe);
        _metrics.push_back(metrics_before);

        //printf("metrics vector length %d, %f, %f\n", _metrics.size(), metrics_before.first, metrics_before.second);
        
        //stop the device
        //stop_viewer(invoke);

        // restart in calibration mode
        //start_viewer(256, 144, 90, invoke);
        stop_realsense();
        start_realsense(256, 144, 90);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // if (speed == 4) // White-wall
        // {
        //     if (_sub->s->supports(RS2_OPTION_VISUAL_PRESET))
        //     {
        //         log("Switching into High-Quality preset for White-Wall mode");
        //         _sub->s->set_option(RS2_OPTION_VISUAL_PRESET, 3.f);
        //         std::this_thread::sleep_for(std::chrono::milliseconds(200));
        //     }
        // }

        calibrate();
        printf("finished autocalibrate!");

        // Get new calibration from the firmware
        std::vector<uint8_t> cmd =
        {
            0x14, 0x00, 0xab, 0xcd,
            0x80, 0x00, 0x00, 0x00,
            13, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };

        auto res = safe_send_command(cmd, "CALIB_RESULT");

        if (res.size() < sizeof(int32_t) + sizeof(DscResultBuffer))
            throw std::runtime_error("Not enough data from CALIB_STATUS!");

        auto result = (DscResultBuffer*)(res.data() + sizeof(int32_t));

        table_header* header = reinterpret_cast<table_header*>(res.data() + sizeof(int32_t) + sizeof(DscResultBuffer));

        if (res.size() < sizeof(int32_t) + sizeof(DscResultBuffer) + sizeof(table_header) + header->table_size)
            throw std::runtime_error("Table truncated in CALIB_STATUS!");

        _new_calib.resize(sizeof(table_header) + header->table_size, 0);
        memcpy(_new_calib.data(), header, _new_calib.size()); // Copy to new_calib

        _health = abs(result->m_dscResultParams.m_healthCheck);

       // log(to_string() << "Calibration completed, health factor = " << _health);

        //stop_viewer(invoke);
        stop_realsense();

        start_realsense(0, 0, 30); // Start with default settings

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Make new calibration active
        apply_calib(true);

       // printf("finished apply calib (false)\n");

        // Capture metrics after
        auto metrics_after = get_depth_metrics(_pipe);
        _metrics.push_back(metrics_after);

        //printf("metrics vector length %d, %f, %f\n", _metrics.size(), metrics_after.first, metrics_after.second);
        printf("rms error before: %f ... rms error after: %f\n", fabs(metrics_before.second), fabs(metrics_after.second));
        std::string message = "rms error after calibration was smaller than before calibration (good!) - would you like to update calibration params (y/n)? (recommended)";

        if (fabs(metrics_before.second) < fabs(metrics_after.second))
        {
            message = "rms error after calibration was bigger than before calibration (NOT good!) - would you like to update calibration params (y/n)? (NOT recommended)";
        }

        std::cout << message << "\n";

        std::string answer;
        std::cin >> answer;

        if (answer == "y")
        {
            printf("updated calibration params\n");
            
            keep();
        }
        else
        {
            printf("Did NOT update calibration params\n");
            apply_calib(false);
        }
        
        

        

    
    }

    void on_chip_calib_cli_manager::restore_workspace(invoker invoke)
    {
        // try
        // {
        //     if (_restored) return;

        //     _viewer.is_3d_view = _in_3d_view;

        //     _viewer.synchronization_enable = _synchronized;

        //     stop_viewer(invoke);

        //     if (_ui.get())
        //     {
        //         _sub->ui = *_ui;
        //         _ui.reset();
        //     }

        //     _sub->post_processing_enabled = _post_processing;

        //     std::this_thread::sleep_for(std::chrono::milliseconds(200));

        //     if (_was_streaming) start_viewer(0, 0, 0, invoke);

        //     _restored = true;
        // }
        // catch (...) {}
    }

    void on_chip_calib_cli_manager::keep()
    {
        // Write new calibration using SETINITCAL command
        uint16_t size = (uint16_t)(0x14 + _new_calib.size());

        std::vector<uint8_t> save_calib{
            0x14, 0, 0xAB, 0xCD, 0x16, 0, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        auto up = (uint16_t*)save_calib.data();
        up[0] = size;

        save_calib.insert(save_calib.end(), _new_calib.data(), _new_calib.data() + _new_calib.size());

        safe_send_command(save_calib, "SETINITCAL");
    }

    void on_chip_calib_cli_manager::apply_calib(bool use_new)
    {
        table_header* hd = (table_header*)(use_new ? _new_calib.data() : _old_calib.data());
        uint8_t* table = (uint8_t*)((use_new ? _new_calib.data() : _old_calib.data()) + sizeof(table_header));

        uint16_t size = (uint16_t)(0x14 + hd->table_size);

        std::vector<uint8_t> apply_calib{
            0x14, 0, 0xAB, 0xCD, 0x51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfe, 0xca, 0xfe, 0xca
        };

        auto up = (uint16_t*)apply_calib.data();
        up[0] = size;

        apply_calib.insert(apply_calib.end(), (uint8_t*)table, ((uint8_t*)table) + hd->table_size);

        safe_send_command(apply_calib, "CALIBRECALC");
    }

    // void autocalib_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    // {
    //     using namespace std;
    //     using namespace chrono;

    //     const auto bar_width = width - 115;

    //     ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

    //     ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
    //     ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
    //     { float(x + width), float(y + 25) }, ImColor(shadow));

    //     if (update_state != RS2_CALIB_STATE_COMPLETE)
    //     {
    //         if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
    //             ImGui::Text("%s", "Calibration Health-Check");
    //         else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS ||
    //                  update_state == RS2_CALIB_STATE_CALIB_COMPLETE ||
    //                  update_state == RS2_CALIB_STATE_SELF_INPUT)
    //             ImGui::Text("%s", "On-Chip Calibration");
    //         else if (update_state == RS2_CALIB_STATE_TARE_INPUT)
    //             ImGui::Text("%s", "Tare Calibration");
    //         if (update_state == RS2_CALIB_STATE_FAILED)
    //             ImGui::Text("%s", "Calibration Failed");

    //         ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });

    //         ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1. - t));

    //         if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
    //         {
    //             ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

    //             ImGui::Text("%s", "Following devices support On-Chip Calibration:");
    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 47) });

    //             ImGui::PushStyleColor(ImGuiCol_Text, white);
    //             ImGui::Text("%s", message.c_str());
    //             ImGui::PopStyleColor();

    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 65) });
    //             ImGui::Text("%s", "Run quick calibration Health-Check? (~30 sec)");
    //         }
    //         else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
    //         {
    //             enable_dismiss = false;
    //             ImGui::Text("%s", "Camera is being calibrated...\nKeep the camera stationary pointing at a wall");
    //         }
    //         else if (update_state == RS2_CALIB_STATE_TARE_INPUT)
    //         {
    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 33) });
    //             ImGui::Text("%s", "Avg Step Count:");

    //             ImGui::SetCursorScreenPos({ float(x + 135), float(y + 30) });

    //             std::string id = to_string() << "##avg_step_count_" << index;
    //             ImGui::PushItemWidth(width - 145);
    //             ImGui::SliderInt(id.c_str(), &get_manager().average_step_count, 1, 30);
    //             ImGui::PopItemWidth();

    //             //-------------------------

    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 38 + ImGui::GetTextLineHeightWithSpacing()) });
    //             ImGui::Text("%s", "Step Count:");

    //             ImGui::SetCursorScreenPos({ float(x + 135), float(y + 35 + ImGui::GetTextLineHeightWithSpacing()) });

    //             id = to_string() << "##step_count_" << index;

    //             ImGui::PushItemWidth(width - 145);
    //             ImGui::SliderInt(id.c_str(), &get_manager().step_count, 1, 30);
    //             ImGui::PopItemWidth();

    //             //-------------------------

    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 43 + 2 * ImGui::GetTextLineHeightWithSpacing()) });
    //             ImGui::Text("%s", "Accuracy:");

    //             ImGui::SetCursorScreenPos({ float(x + 135), float(y + 40 + 2 * ImGui::GetTextLineHeightWithSpacing()) });

    //             id = to_string() << "##accuracy_" << index;

    //             std::vector<std::string> vals{ "Very High", "High", "Medium", "Low" };
    //             std::vector<const char*> vals_cstr;
    //             for (auto&& s : vals) vals_cstr.push_back(s.c_str());

    //             ImGui::PushItemWidth(width - 145);
    //             ImGui::Combo(id.c_str(), &get_manager().accuracy, vals_cstr.data(), vals.size());
    //             ImGui::PopItemWidth();

    //             //-------------------------

    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 48 + 3 * ImGui::GetTextLineHeightWithSpacing()) });
    //             ImGui::Text("%s", "Ground Truth(mm):");

    //             ImGui::SetCursorScreenPos({ float(x + 135), float(y + 45 + 3 * ImGui::GetTextLineHeightWithSpacing()) });

    //             id = to_string() << "##ground_truth_for_tare" << index;

    //             std::string gt = to_string() << get_manager().ground_truth;
    //             const int MAX_SIZE = 256;
    //             char buff[MAX_SIZE];
    //             memcpy(buff, gt.c_str(), gt.size() + 1);

    //             ImGui::PushItemWidth(width - 145);
    //             if (ImGui::InputText(id.c_str(), buff, std::max((int)gt.size() + 1, 10)))
    //             {
    //                 std::stringstream ss;
    //                 ss << buff;
    //                 ss >> get_manager().ground_truth;
    //             }
    //             ImGui::PopItemWidth();

    //             auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

    //             ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
    //             ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

    //             std::string button_name = to_string() << "Calibrate" << "##tare" << index;

    //             ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
    //             if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
    //             {
    //                 get_manager().restore_workspace([this](std::function<void()> a){ a(); });
    //                 get_manager().reset();
    //                 get_manager().tare = true;
    //                 get_manager().start(shared_from_this());
    //                 update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
    //                 enable_dismiss = false;
    //             }

    //             ImGui::PopStyleColor(2);

    //             if (ImGui::IsItemHovered())
    //             {
    //                 ImGui::SetTooltip("%s", "Begin Tare Calibration");
    //             }
    //         }
    //         else if (update_state == RS2_CALIB_STATE_SELF_INPUT)
    //         {
    //             ImGui::SetCursorScreenPos({ float(x + 9), float(y + 33) });
    //             ImGui::Text("%s", "Speed:");

    //             ImGui::SetCursorScreenPos({ float(x + 135), float(y + 30) });

    //             std::string id = to_string() << "##speed_" << index;

    //             std::vector<std::string> vals{ "Very Fast", "Fast", "Medium", "Slow", "White Wall" };
    //             std::vector<const char*> vals_cstr;
    //             for (auto&& s : vals) vals_cstr.push_back(s.c_str());

    //             ImGui::PushItemWidth(width - 145);
    //             ImGui::Combo(id.c_str(), &get_manager().speed, vals_cstr.data(), vals.size());
    //             ImGui::PopItemWidth();

    //             auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

    //             ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
    //             ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

    //             std::string button_name = to_string() << "Calibrate" << "##self" << index;

    //             ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
    //             if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
    //             {
    //                 get_manager().restore_workspace([this](std::function<void()> a) { a(); });
    //                 get_manager().reset();
    //                 get_manager().start(shared_from_this());
    //                 update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
    //                 enable_dismiss = false;
    //             }

    //             ImGui::PopStyleColor(2);

    //             if (ImGui::IsItemHovered())
    //             {
    //                 ImGui::SetTooltip("%s", "Begin On-Chip Calibration");
    //             }
    //         }
    //         else if (update_state == RS2_CALIB_STATE_FAILED)
    //         {
    //             ImGui::Text("%s", _error_message.c_str());

    //             auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

    //             ImGui::PushStyleColor(ImGuiCol_Button, saturate(redish, sat));
    //             ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(redish, 1.5f));

    //             std::string button_name = to_string() << "Retry" << "##retry" << index;

    //             ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
    //             if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
    //             {
    //                 get_manager().restore_workspace([this](std::function<void()> a){ a(); });
    //                 get_manager().reset();
    //                 get_manager().start(shared_from_this());
    //                 update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
    //                 enable_dismiss = false;
    //             }

    //             ImGui::PopStyleColor(2);

    //             if (ImGui::IsItemHovered())
    //             {
    //                 ImGui::SetTooltip("%s", "Retry on-chip calibration process");
    //             }
    //         }
    //         else if (update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
    //         {
    //             auto health = get_manager().get_health();

    //             auto recommend_keep = get_manager().allow_calib_keep();

    //             ImGui::SetCursorScreenPos({ float(x + 15), float(y + 33) });

    //             if (get_manager().tare)
    //             {
    //                 ImGui::Text("%s", "Tare calibration complete:");
    //             }
    //             else
    //             {
    //                 ImGui::Text("%s", "Calibration Error: ");

    //                 std::stringstream ss; ss << std::fixed << std::setprecision(2) << health;
    //                 auto health_str = ss.str();

    //                 std::string text_name = to_string() << "##notification_text_" << index;
                    
    //                 ImGui::SetCursorScreenPos({ float(x + 136), float(y + 30) });
    //                 ImGui::PushStyleColor(ImGuiCol_Text, white);
    //                 ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
    //                 ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
    //                 ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
    //                 ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
    //                 ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
    //                 ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
    //                 ImGui::InputTextMultiline(text_name.c_str(), const_cast<char*>(health_str.c_str()),
    //                     strlen(health_str.c_str()) + 1, { 50, 
    //                                                     ImGui::GetTextLineHeight() + 6 },
    //                     ImGuiInputTextFlags_ReadOnly);
    //                 ImGui::PopStyleColor(7);

    //                 ImGui::SetCursorScreenPos({ float(x + 172), float(y + 33) });

    //                 if (!recommend_keep)
    //                 {
    //                     ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
    //                     ImGui::Text("%s", "(Good)");
    //                 }
    //                 else if (health < 0.25f)
    //                 {
    //                     ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
    //                     ImGui::Text("%s", "(Can be Improved)");
    //                 }
    //                 else  
    //                 {
    //                     ImGui::PushStyleColor(ImGuiCol_Text, redish);
    //                     ImGui::Text("%s", "(Requires Calibration)");
    //                 }
    //                 ImGui::PopStyleColor();

    //                 if (ImGui::IsItemHovered())
    //                 {
    //                     ImGui::SetTooltip("%s", "Calibration error captures how far camera calibration is from the optimal one\n"
    //                         "[0, 0.15) - Good\n"
    //                         "[0.15, 0.25) - Can be Improved\n"
    //                         "[0.25, ) - Requires Calibration");
    //                 }
    //             }

    //             if (recommend_keep)
    //             {
                
    //                 auto old_fr = get_manager().get_metric(false).first;
    //                 auto new_fr = get_manager().get_metric(true).first;

    //                 auto old_rms = fabs(get_manager().get_metric(false).second);
    //                 auto new_rms = fabs(get_manager().get_metric(true).second);

    //                 auto fr_improvement = 100.f * ((new_fr - old_fr) / old_fr);
    //                 auto rms_improvement = 100.f * ((old_rms - new_rms) / old_rms);

    //                 std::string old_units = "mm";
    //                 if (old_rms > 10.f)
    //                 {
    //                     old_rms /= 10.f;
    //                     old_units = "cm";
    //                 }
    //                 std::string new_units = "mm";
    //                 if (new_rms > 10.f)
    //                 {
    //                     new_rms /= 10.f;
    //                     new_units = "cm";
    //                 }

    //                 // NOTE: Disabling metrics temporarily
    //                 // TODO: Re-enable in future release
    //                 if (/* fr_improvement > 1.f || rms_improvement > 1.f */ false)
    //                 {
    //                     std::string txt = to_string() << "  Fill-Rate: " << std::setprecision(1) << std::fixed << new_fr << "%%";

    //                     if (!use_new_calib)
    //                     {
    //                         txt = to_string() << "  Fill-Rate: " << std::setprecision(1) << std::fixed << old_fr << "%%\n";
    //                     }

    //                     ImGui::SetCursorScreenPos({ float(x + 12), float(y + 90) });
    //                     ImGui::PushFont(win.get_large_font());
    //                     ImGui::Text("%s", static_cast<const char *>(textual_icons::check));
    //                     ImGui::PopFont();

    //                     ImGui::SetCursorScreenPos({ float(x + 35), float(y + 92) });
    //                     ImGui::Text("%s", txt.c_str());

    //                     if (use_new_calib)
    //                     {
    //                         ImGui::SameLine();

    //                         ImGui::PushStyleColor(ImGuiCol_Text, white);
    //                         txt = to_string() << " ( +" << std::fixed << std::setprecision(0) << fr_improvement << "%% )";
    //                         ImGui::Text("%s", txt.c_str());
    //                         ImGui::PopStyleColor();
    //                     }

    //                     if (rms_improvement > 1.f)
    //                     {
    //                         if (use_new_calib)
    //                         {
    //                             txt = to_string() << "  Noise Estimate: " << std::setprecision(2) << std::fixed << new_rms << new_units;
    //                         }
    //                         else
    //                         {
    //                             txt = to_string() << "  Noise Estimate: " << std::setprecision(2) << std::fixed << old_rms << old_units;
    //                         }

    //                         ImGui::SetCursorScreenPos({ float(x + 12), float(y + 90 + ImGui::GetTextLineHeight() + 6) });
    //                         ImGui::PushFont(win.get_large_font());
    //                         ImGui::Text("%s", static_cast<const char *>(textual_icons::check));
    //                         ImGui::PopFont();

    //                         ImGui::SetCursorScreenPos({ float(x + 35), float(y + 92 + ImGui::GetTextLineHeight() + 6) });
    //                         ImGui::Text("%s", txt.c_str());

    //                         if (use_new_calib)
    //                         {
    //                             ImGui::SameLine();

    //                             ImGui::PushStyleColor(ImGuiCol_Text, white);
    //                             txt = to_string() << " ( -" << std::setprecision(0) << std::fixed << rms_improvement << "%% )";
    //                             ImGui::Text("%s", txt.c_str());
    //                             ImGui::PopStyleColor();
    //                         }
    //                     }
    //                 }
    //                 else
    //                 {
    //                     ImGui::SetCursorScreenPos({ float(x + 12), float(y + 100) });
    //                     ImGui::Text("%s", "Please compare new vs old calibration\nand decide if to keep or discard the result...");
    //                 }

    //                 ImGui::SetCursorScreenPos({ float(x + 9), float(y + 60) });

    //                 if (ImGui::RadioButton("New", use_new_calib))
    //                 {
    //                     use_new_calib = true;
    //                     get_manager().apply_calib(true);
    //                 }

    //                 ImGui::SetCursorScreenPos({ float(x + 150), float(y + 60) });
    //                 if (ImGui::RadioButton("Original", !use_new_calib))
    //                 {
    //                     use_new_calib = false;
    //                     get_manager().apply_calib(false);
    //                 }

    //                 auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                
    //                 ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
    //                 ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));

    //                 std::string button_name = to_string() << "Apply New" << "##apply" << index;
    //                 if (!use_new_calib) button_name = to_string() << "Keep Original" << "##original" << index;

    //                 ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
    //                 if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
    //                 {
    //                     if (use_new_calib)
    //                     {
    //                         get_manager().keep();
    //                         update_state = RS2_CALIB_STATE_COMPLETE;
    //                         pinned = false;
    //                         enable_dismiss = false;
    //                         last_progress_time = last_interacted = system_clock::now() + milliseconds(500);
    //                     }
    //                     else dismiss(false);

    //                     get_manager().restore_workspace([this](std::function<void()> a) { a(); });
    //                 }

    //                 ImGui::PopStyleColor(2);

    //                 if (ImGui::IsItemHovered())
    //                 {
    //                     ImGui::SetTooltip("%s", "New calibration values will be saved in device memory");
    //                 }
    //             }
    //         }

    //         ImGui::PopStyleColor();
    //     }
    //     else
    //     {
    //         ImGui::Text("%s", "Calibration Complete");

    //         ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
    //         ImGui::PushFont(win.get_large_font());
    //         std::string txt = to_string() << textual_icons::throphy;
    //         ImGui::Text("%s", txt.c_str());
    //         ImGui::PopFont();

    //         ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });

    //         ImGui::Text("%s", "Camera Calibration Applied Successfully");
    //     }

    //     ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

    //     if (update_manager)
    //     {
    //         if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
    //         {
    //             auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
    //             ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
    //             ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
    //             std::string button_name = to_string() << "Health-Check" << "##health_check" << index;

    //             if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }) || update_manager->started())
    //             {
    //                 if (!update_manager->started()) update_manager->start(shared_from_this());

    //                 update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
    //                 enable_dismiss = false;
    //                 last_progress_time = system_clock::now();
    //             }
    //             ImGui::PopStyleColor(2);

    //             if (ImGui::IsItemHovered())
    //             {
    //                 ImGui::SetTooltip("%s", "Keep the camera pointing at an object or a wall");
    //             }
    //         }
    //         else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
    //         {
    //             if (update_manager->done())
    //             {
    //                 update_state = RS2_CALIB_STATE_CALIB_COMPLETE;
    //                 enable_dismiss = true;
    //                 get_manager().apply_calib(true);
    //                 use_new_calib = true;
    //             }

    //             if (!expanded)
    //             {
    //                 if (update_manager->failed())
    //                 {
    //                     update_manager->check_error(_error_message);
    //                     update_state = RS2_CALIB_STATE_FAILED;
    //                     enable_dismiss = true;
    //                     //pinned = false;
    //                     //dismiss(false);
    //                 }

    //                 draw_progress_bar(win, bar_width);

    //                 ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

    //                 string id = to_string() << "Expand" << "##" << index;
    //                 ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
    //                 if (ImGui::Button(id.c_str(), { 100, 20 }))
    //                 {
    //                     expanded = true;
    //                 }

    //                 ImGui::PopStyleColor();
    //             }
    //         }
    //     }
    //}

    //void autocalib_notification_model::dismiss(bool snooze)
   // {
        // get_manager().update_last_used();

        // if (!use_new_calib && get_manager().done()) 
        //     get_manager().apply_calib(false);

        // get_manager().restore_workspace([this](std::function<void()> a){ a(); });

        // if (update_state != RS2_CALIB_STATE_TARE_INPUT)
        //     update_state = RS2_CALIB_STATE_INITIAL_PROMPT;
        // get_manager().reset();

        // notification_model::dismiss(snooze);
    //}

   // void autocalib_notification_model::draw_expanded(ux_window& win, std::string& error_message)
    //{
        // if (update_manager->started() && update_state == RS2_CALIB_STATE_INITIAL_PROMPT) 
        //     update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;

        // auto flags = ImGuiWindowFlags_NoResize |
        //     ImGuiWindowFlags_NoMove |
        //     ImGuiWindowFlags_NoCollapse;

        // ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
        // ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        // ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        // ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);

        // ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(500, 100));
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        // std::string title = "On-Chip Calibration";
        // if (update_manager->failed()) title += " Failed";

        // ImGui::OpenPopup(title.c_str());
        // if (ImGui::BeginPopupModal(title.c_str(), nullptr, flags))
        // {
        //     ImGui::SetCursorPosX(200);
        //     std::string progress_str = to_string() << "Progress: " << update_manager->get_progress() << "%";
        //     ImGui::Text("%s", progress_str.c_str());

        //     ImGui::SetCursorPosX(5);

        //     draw_progress_bar(win, 490);

        //     ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
        //     auto s = update_manager->get_log();
        //     ImGui::InputTextMultiline("##autocalib_log", const_cast<char*>(s.c_str()),
        //         s.size() + 1, { 490,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
        //     ImGui::PopStyleColor();

        //     ImGui::SetCursorPosX(190);
        //     if (visible || update_manager->done() || update_manager->failed())
        //     {
        //         if (ImGui::Button("OK", ImVec2(120, 0)))
        //         {
        //             if (update_manager->failed())
        //             {
        //                 update_state = RS2_CALIB_STATE_FAILED;
        //                 //pinned = false;
        //                 //dismiss(false);
        //             }
        //             expanded = false;
        //             ImGui::CloseCurrentPopup();
        //         }
        //     }
        //     else
        //     {
        //         ImGui::PushStyleColor(ImGuiCol_Button, transparent);
        //         ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
        //         ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
        //         ImGui::PushStyleColor(ImGuiCol_Text, transparent);
        //         ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, transparent);
        //         ImGui::Button("OK", ImVec2(120, 0));
        //         ImGui::PopStyleColor(5);
        //     }

        //     ImGui::EndPopup();
        // }

        // ImGui::PopStyleVar(3);
        // ImGui::PopStyleColor(4);

        // error_message = "";
    //}

   // int autocalib_notification_model::calc_height()
    //{
        // if (update_state == RS2_CALIB_STATE_COMPLETE) return 65;
        // else if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT) return 120;
        // else if (update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
        // {
        //     if (get_manager().allow_calib_keep()) return 170;
        //     else return 80;
        // }
        // else if (update_state == RS2_CALIB_STATE_SELF_INPUT) return 85;
        // else if (update_state == RS2_CALIB_STATE_TARE_INPUT) return 160;
        // else return 100;
    //}

    //void autocalib_notification_model::set_color_scheme(float t) const
   // {
        // notification_model::set_color_scheme(t);

        // ImGui::PopStyleColor(1);

        // ImVec4 c;

        // if (update_state == RS2_CALIB_STATE_COMPLETE)
        // {
        //     c = alpha(saturate(light_blue, 0.7f), 1 - t);
        //     ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        // }
        // else if (update_state == RS2_CALIB_STATE_FAILED)
        // {
        //     c = alpha(dark_red, 1 - t);
        //     ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        // }
        // else
        // {
        //     c = alpha(sensor_bg, 1 - t);
        //     ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        // }
    //}


}