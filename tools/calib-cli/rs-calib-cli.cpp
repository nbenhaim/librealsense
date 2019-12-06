// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include <numeric>
#include <librealsense2/rs.hpp>
#include "viewer.h"
#include "on-chip-calib-cli.h"

#include "tclap/CmdLine.h"
#include "tclap/ValueArg.h"

using namespace TCLAP;

void stop_realsense(rs2::pipeline pipe)
{
    pipe.stop();

}

rs2::device init()
{
    rs2::context _ctx;
    
    rs2::device _dev;
    bool valid_config = false;
   
    rs2::device d;
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

            d = dev;
            
        }
    }
    else
    {
        printf("No realsense devices found!\n");
        return d;
    }

    return d;
    
}

bool load_calib_data(rs2::device dev, const char * filepath)
{
    rs2::on_chip_calib_cli_manager calib = rs2::on_chip_calib_cli_manager(dev);

    calib.load_calib(filepath);

    

    return true;
}

bool save_calib_data(rs2::device dev, const char * filepath)
{
    rs2::on_chip_calib_cli_manager calib = rs2::on_chip_calib_cli_manager(dev);

    calib.save_calib(filepath);

    return true;
}

bool on_chip_calibrate(rs2::device dev)
{
    

    rs2::on_chip_calib_cli_manager calib = rs2::on_chip_calib_cli_manager(dev);

    calib.process_flow();
    
    
    return true;
}
int main(int argc, const char * argv[]) try
{

    CmdLine cmd("librealsense on chip tool", ' ', RS2_API_VERSION_STR);

    SwitchArg calibrate_arg("c", "calibrate", "runs on-chip calibration");
  
    ValueArg<std::string> save_arg("s", "save", "saves calibration params to a file specified here", false, "", "string");
    ValueArg<std::string> load_arg("l", "load", "loads calibration params from a file", false, "", "string");


    cmd.add(calibrate_arg);
    cmd.add(save_arg);
    cmd.add(load_arg);
   
    cmd.parse(argc, argv);
    
    if (!calibrate_arg.isSet() && !save_arg.isSet() && !load_arg.isSet())
    {
        std::cout << std::endl << "nothing to do, run again with -h for help" << std::endl;
        return EXIT_FAILURE;
    }

    rs2::device dev = init();

    if (calibrate_arg.isSet())
    {
        bool valid_config = on_chip_calibrate(dev);
    }
    else if(save_arg.isSet())
    {
        std::string save_file;
        save_file = save_arg.getValue();
        save_calib_data(dev, save_file.c_str());
    }
    else if(load_arg.isSet())
    {
        std::string load_file;
        load_file = load_arg.getValue();
        load_calib_data(dev, load_file.c_str());
    }

    
    
    
 
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
