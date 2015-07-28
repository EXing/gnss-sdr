/*
 * beidou_b1i_pcps_acquisition.cc
 *
 *  Created on: Jul 15, 2015
 *      Author: giorgio
 */

#include "beidou_b1i_pcps_acquisition.h"
#include <iostream>
#include <stdexcept>
#include <boost/math/distributions/exponential.hpp>
#include <glog/logging.h>
#include <gnuradio/msg_queue.h>
#include "beidou_sdr_signal_processing.h"
#include "BEIDOU_B1I.h"
#include "configuration_interface.h"


using google::LogMessage;

BeidouB1iPcpsAcquisition::BeidouB1iPcpsAcquisition(
        ConfigurationInterface* configuration, std::string role,
        unsigned int in_streams, unsigned int out_streams,
        gr::msg_queue::sptr queue) :
    role_(role), in_streams_(in_streams), out_streams_(out_streams), queue_(queue)
{
    configuration_ = configuration;
    std::string default_item_type     =      "gr_complex";
    std::string default_dump_filename =  "./data/acquisition.dat";

    DLOG(INFO) << "role " << role;

    item_type_ = configuration_->property(role + ".item_type", default_item_type);
    //float pfa =  configuration_->property(role + ".pfa", 0.0);

    fs_in_ = configuration_->property("GNSS-SDR.internal_fs_hz", 2048000);
    if_ = configuration_->property(role + ".ifreq", 0);
    dump_ = configuration_->property(role + ".dump", false);
    shift_resolution_ = configuration_->property(role + ".doppler_max", 15);
    sampled_ms_ = configuration_->property(role + ".coherent_integration_time_ms", 1);

    bit_transition_flag_ = configuration_->property(role + ".bit_transition_flag", false);

    if (!bit_transition_flag_)
        {
            max_dwells_ = configuration_->property(role + ".max_dwells", 1);
        }
    else
        {
            max_dwells_ = 2;
        }

    dump_filename_ = configuration_->property(role + ".dump_filename", default_dump_filename);

    //--- Find number of samples per spreading code -------------------------
    code_length_ = round(fs_in_ / (BEIDOU_B1I_CODE_RATE_HZ / BEIDOU_B1I_CODE_LENGTH_CHIPS));

    vector_length_ = code_length_ * sampled_ms_;

    code_= new gr_complex[vector_length_];

    // if (item_type_.compare("gr_complex") == 0 )
    //         {
    item_size_ = sizeof(gr_complex);
    acquisition_cc_ = pcps_make_acquisition_cc(sampled_ms_, max_dwells_,
            shift_resolution_, if_, fs_in_, code_length_, code_length_,
            bit_transition_flag_, queue_, dump_, dump_filename_);

    stream_to_vector_ = gr::blocks::stream_to_vector::make(item_size_, vector_length_);

    DLOG(INFO) << "stream_to_vector(" << stream_to_vector_->unique_id() << ")";
    DLOG(INFO) << "acquisition(" << acquisition_cc_->unique_id() << ")";
    //        }

    if (item_type_.compare("cshort") == 0)
        {
            cshort_to_float_x2_ = make_cshort_to_float_x2();
            float_to_complex_ = gr::blocks::float_to_complex::make();
        }

    if (item_type_.compare("cbyte") == 0)
        {
            cbyte_to_float_x2_ = make_complex_byte_to_float_x2();
            float_to_complex_ = gr::blocks::float_to_complex::make();
        }
    //}
    //else
    // {
    //     LOG(WARNING) << item_type_
    //             << " unknown acquisition item type";
    // }
    channel_      = 0;
    threshold_    = 0.0;
    doppler_max_  = 0;
    doppler_step_ = 0;
    gnss_synchro_ = 0;
    channel_internal_queue_ = 0;
}


BeidouB1iPcpsAcquisition::~BeidouB1iPcpsAcquisition()
{
    delete[] code_;
}


void BeidouB1iPcpsAcquisition::set_channel(unsigned int channel)
{
    channel_ = channel;
    //if (item_type_.compare("gr_complex") == 0)
    //{
    acquisition_cc_->set_channel(channel_);
    //}
}


void BeidouB1iPcpsAcquisition::set_threshold(float threshold)
{
    float pfa = configuration_->property(role_ + boost::lexical_cast<std::string>(channel_) + ".pfa", 0.0);

    if(pfa == 0.0)
        {
            pfa = configuration_->property(role_ + ".pfa", 0.0);
            threshold_ = threshold;
        }
    else
        {
            threshold_ = calculate_threshold(pfa);
        }

    DLOG(INFO) << "Channel " << channel_ << " Threshold = " << threshold_;

   // if (item_type_.compare("gr_complex") == 0)
    //    {
            acquisition_cc_->set_threshold(threshold_);
    //    }
}


void BeidouB1iPcpsAcquisition::set_doppler_max(unsigned int doppler_max)
{
    doppler_max_ = doppler_max;
    //   if (item_type_.compare("gr_complex") == 0)
    //  {
    acquisition_cc_->set_doppler_max(doppler_max_);
    // }
}


void BeidouB1iPcpsAcquisition::set_doppler_step(unsigned int doppler_step)
{
    doppler_step_ = doppler_step;
    //   if (item_type_.compare("gr_complex") == 0)
    //      {
    acquisition_cc_->set_doppler_step(doppler_step_);
    //     }

}


void BeidouB1iPcpsAcquisition::set_channel_queue(
        concurrent_queue<int> *channel_internal_queue)
{
    channel_internal_queue_ = channel_internal_queue;
    //  if (item_type_.compare("gr_complex") == 0)
    //  {
    acquisition_cc_->set_channel_queue(channel_internal_queue_);
    //  }
}


void BeidouB1iPcpsAcquisition::set_gnss_synchro(Gnss_Synchro* gnss_synchro)
{
    gnss_synchro_ = gnss_synchro;
    // if (item_type_.compare("gr_complex") == 0)
    // {
    acquisition_cc_->set_gnss_synchro(gnss_synchro_);
    // }
}


signed int BeidouB1iPcpsAcquisition::mag()
{
    // //    if (item_type_.compare("gr_complex") == 0)
    //        {
    return acquisition_cc_->mag();
    //       }
    //   else
    //       {
    //           return 0;
    //      }
}


void BeidouB1iPcpsAcquisition::init()
{
    acquisition_cc_->init();
    set_local_code();
}


void BeidouB1iPcpsAcquisition::set_local_code()
{
    // if (item_type_.compare("gr_complex") == 0)
    //   {
    std::complex<float>* code = new std::complex<float>[code_length_];

    beidou_b1i_code_gen_complex_sampled(code, gnss_synchro_->PRN, fs_in_, 0);   // DA MODIFICARE

    for (unsigned int i = 0; i < sampled_ms_; i++)
        {
            memcpy(&(code_[i*code_length_]), code,
                    sizeof(gr_complex)*code_length_);
        }

    acquisition_cc_->set_local_code(code_);

    delete[] code;
    //  }
}


void BeidouB1iPcpsAcquisition::reset()
{
    //  if (item_type_.compare("gr_complex") == 0)
    //  {
    acquisition_cc_->set_active(true);
    //  }
}

void BeidouB1iPcpsAcquisition::set_state(int state)
{
    //  if (item_type_.compare("gr_complex") == 0)
    //  {
    acquisition_cc_->set_state(state);
    //  }
}



float BeidouB1iPcpsAcquisition::calculate_threshold(float pfa)
{
    //Calculate the threshold
    unsigned int frequency_bins = 0;
    for (int doppler = (int)(-doppler_max_); doppler <= (int)doppler_max_; doppler += doppler_step_)
        {
            frequency_bins++;
        }
    DLOG(INFO) << "Channel " << channel_<< "  Pfa = " << pfa;
    unsigned int ncells = vector_length_ * frequency_bins;
    double exponent = 1 / static_cast<double>(ncells);
    double val = pow(1.0 - pfa, exponent);
    double lambda = double(vector_length_);
    boost::math::exponential_distribution<double> mydist (lambda);
    float threshold = (float)quantile(mydist,val);

    return threshold;
}


void BeidouB1iPcpsAcquisition::connect(gr::top_block_sptr top_block)
{
    if (item_type_.compare("gr_complex") == 0)
        {
            top_block->connect(stream_to_vector_, 0, acquisition_cc_, 0);
        }
    else if (item_type_.compare("cshort") == 0)
        {
            top_block->connect(cshort_to_float_x2_, 0, float_to_complex_, 0);
            top_block->connect(cshort_to_float_x2_, 1, float_to_complex_, 1);
            top_block->connect(float_to_complex_, 0, stream_to_vector_, 0);
            top_block->connect(stream_to_vector_, 0, acquisition_cc_, 0);
        }
    else if (item_type_.compare("cbyte") == 0)
        {
            top_block->connect(cbyte_to_float_x2_, 0, float_to_complex_, 0);
            top_block->connect(cbyte_to_float_x2_, 1, float_to_complex_, 1);
            top_block->connect(float_to_complex_, 0, stream_to_vector_, 0);
            top_block->connect(stream_to_vector_, 0, acquisition_cc_, 0);
        }
    else
        {
            LOG(WARNING) << item_type_ << " unknown acquisition item type";
        }

}


void BeidouB1iPcpsAcquisition::disconnect(gr::top_block_sptr top_block)
{
    if (item_type_.compare("gr_complex") == 0)
        {
            top_block->disconnect(stream_to_vector_, 0, acquisition_cc_, 0);
        }
    else if (item_type_.compare("cshort") == 0)
        {
            // Since a short-based acq implementation is not available,
            // we just convert cshorts to gr_complex
            top_block->disconnect(cshort_to_float_x2_, 0, float_to_complex_, 0);
            top_block->disconnect(cshort_to_float_x2_, 1, float_to_complex_, 1);
            top_block->disconnect(float_to_complex_, 0, stream_to_vector_, 0);
            top_block->disconnect(stream_to_vector_, 0, acquisition_cc_, 0);
        }
    else if (item_type_.compare("cbyte") == 0)
        {
            // Since a byte-based acq implementation is not available,
            // we just convert cshorts to gr_complex
            top_block->disconnect(cbyte_to_float_x2_, 0, float_to_complex_, 0);
            top_block->disconnect(cbyte_to_float_x2_, 1, float_to_complex_, 1);
            top_block->disconnect(float_to_complex_, 0, stream_to_vector_, 0);
            top_block->disconnect(stream_to_vector_, 0, acquisition_cc_, 0);
        }
    else
        {
            LOG(WARNING) << item_type_ << " unknown acquisition item type";
        }
}


gr::basic_block_sptr BeidouB1iPcpsAcquisition::get_left_block()
{
    if (item_type_.compare("gr_complex") == 0)
        {
            return stream_to_vector_;
        }
    else if (item_type_.compare("cshort") == 0)
        {
            return cshort_to_float_x2_;
        }
    else if (item_type_.compare("cbyte") == 0)
        {
            return cbyte_to_float_x2_;
        }
    else
        {
            LOG(WARNING) << item_type_ << " unknown acquisition item type";
            return nullptr;
        }
}


gr::basic_block_sptr BeidouB1iPcpsAcquisition::get_right_block()
{
    return acquisition_cc_;
}




