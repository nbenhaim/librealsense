// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include <numeric>
#include <librealsense2/rs.hpp>
#include "viewer.h"
#include "on-chip-calib-cli.h"



void stop_realsense(rs2::pipeline pipe)
{
    pipe.stop();

}

bool init()
{
    
    

    rs2::context _ctx;
    
    rs2::device _dev;
    bool valid_config = false;
   

    // Adjust settings according to USB type
    bool usb3_device = true;
    
    auto devices = _ctx.query_devices();
    
    if (devices.size())
    {
        
        auto dev = devices[0];

        bool usb3_device = true;
        if (dev.supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
        {
        
            std::string usb_type = dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
            
            usb3_device = !(std::string::npos != usb_type.find("2."));

            _dev = dev;
            
        }
    }
    else
    {
        printf("No realsense devices found!\n");
        return valid_config;
    }
    
    int requested_fps = usb3_device ? 30 : 15;

    
    //rs2::pipeline pipe = start_realsense(_dev, 0, 0, requested_fps);
    

    rs2::on_chip_calib_cli_manager calib = rs2::on_chip_calib_cli_manager(_dev);

    calib.process_flow();
    
    
    return valid_config;
}
int main(int argc, const char * argv[]) try
{
    
    bool valid_config = init();
    

 
    return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
