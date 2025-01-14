#include <getopt.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>

#include "frame_input.hh"
#include "ivf_reader.hh"
#include "yuv4mpeg.hh"
#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "encoder.hh"
#include "macroblock.hh"
#include "ivf_writer.hh"
#include "enc_state_serializer.hh"
#include "cpp_function.hpp"

using namespace std;

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        try {
            bool two_pass = false;
            double kf_q_weight = 0.75;
            bool extra_frame_chunk = false;
            EncoderQuality quality = BEST_QUALITY;
            uint8_t * _ivf = reinterpret_cast <uint8_t *> (arg_values[0]);  // bucket1 worker_id 1
            int worker_id = (int) _ivf[0];
            int act_id = (int) _ivf[1];
            uint8_t * prev_ivf = _ivf + 2;
            // char * act_id_ = arg_values[1];     // bucket1 act_id    1
            // char * prev_ivf_ = arg_values[2];   // bucket1 ivf0
            char * prev_state_ = arg_values[1]; // bucket1 state0
            char * prev_ivf_state_ = NULL;
            uint8_t * prev_ivf_state = NULL;

            if ( act_id == 3 ) {
                prev_ivf_state_ = arg_values[2];
                prev_ivf_state = reinterpret_cast <uint8_t *> (prev_ivf_state_);
            } 
            // int worker_id = reinterpret_cast <int *> (worker_id_)[0];
            // int act_id = reinterpret_cast <int *> (act_id_) [0];
            // uint8_t * prev_ivf = reinterpret_cast <uint8_t *> (prev_ivf_);
            uint8_t * prev_state = reinterpret_cast <uint8_t *> (prev_state_);
            cout << "worker id and act id are: " << worker_id << " " << act_id << endl;
            std::string file_id = std::to_string(worker_id + 1);
            while (file_id.size() < 8) file_id = "0" + file_id;
            std::string input_y4m = "/data/sintel-1k-y4m_06/" + file_id + ".y4m";

            shared_ptr<FrameInput> input_reader;
            input_reader = make_shared<YUV4MPEGReader>( input_y4m );

            Decoder pred_decoder( input_reader->display_width(), input_reader->display_height() );

            if ( arg_size == 5 ) {
                int iter = 0;
                pred_decoder = Decoder::deserialize_shm( prev_ivf_state, iter );
            }
            
            string ivf_bucket = "bucket" +std::to_string(worker_id);
            string ivf_key = "ivf" + std::to_string(act_id);
            IVFShmWriter output{library, worker_id, act_id, "VP80", input_reader->display_width(), input_reader->display_height(), 1, 1};
            
            cout << "ivf shm writer has been constructed " << endl;
            vector<RasterHandle> original_rasters;
            for ( size_t i = 0; ; i++ ) {
                auto next_raster = input_reader->get_next_frame();

                if ( next_raster.initialized() ) {
                    original_rasters.emplace_back( next_raster.get() );
                } else {
                    break;
                }
            }
            

            vector<pair<Optional<KeyFrame>, Optional<InterFrame> > > prediction_frames;
            cout << "..." << endl;
            IVFShm pred_ivf {prev_ivf};
            

            if ( not pred_decoder.minihash_match( pred_ivf.expected_decoder_minihash() ) ) {
                throw Invalid( "Mismatch between prediction IVF and prediction_ivf_initial_state" );
                cout <<  "Mismatch between prediction IVF and prediction_ivf_initial_state"  << endl;
            }
            cout << "frame count is: " << pred_ivf.frame_count() << endl;
            for ( unsigned int i = 0; i < pred_ivf.frame_count(); i++) {
                UncompressedChunk unch { pred_ivf.frame( i ), pred_ivf.width(), pred_ivf.height(), false };
                if ( unch.key_frame() ) {
                    KeyFrame frame = pred_decoder.parse_frame<KeyFrame>( unch );
                    pred_decoder.decode_frame( frame );
                    prediction_frames.emplace_back( move( frame ), Optional<InterFrame>() );
                } else {
                    InterFrame frame = pred_decoder.parse_frame<InterFrame>( unch );
                    pred_decoder.decode_frame( frame );
                    prediction_frames.emplace_back( Optional<KeyFrame>(), move( frame ) );
                }
            }
            int iter = 0;

            Encoder encoder( Decoder::deserialize_shm( prev_state, iter ),
                             two_pass, quality );
            cout << "finish deserialize from prev state in shared mem" << endl;
            output.set_expected_decoder_entry_hash( encoder.export_decoder().get_hash().hash() );
            cout << iter << endl;
            encoder.reencode_shm( original_rasters, prediction_frames, kf_q_weight,
                                  extra_frame_chunk, output );
            output.send();
            
            // int size = 4;
            // string bucket = "bucket" + std::to_string( worker_id );
            // string key = "worker_id";
            // auto worker_obj = library->create_object_with_bucket_key(bucket, key, size);
            // int * worker_id_shm = reinterpret_cast<int *>(worker_obj->get_value());
            // worker_id_shm[0] = worker_id;
            // library->send_object(worker_obj);

            // key = "act_id";
            // auto act_obj = library->create_object_with_bucket_key(bucket, key, size);
            // int * act_id_shm = reinterpret_cast<int *>(act_obj->get_value());
            // act_id_shm[0] = act_id + 1;
            // library->send_object(act_obj);
            string target_func ("xc-enc");
            // bucket = "bucket" + std::to_string(worker_id + 1);
            // key = "state" + std::to_string(act_id);
            int size = encoder.export_decoder().get_len();
            auto state_obj = library->create_object_with_target(target_func, (worker_id*worker_id + worker_id) / 2 + act_id, size);
            // auto state_obj = library->create_object_with_bucket_key(bucket, key, size);
            uint8_t * state_shm = reinterpret_cast<uint8_t *> (state_obj->get_value());
            iter = 0;
            encoder.export_decoder().serialize_shm(state_shm, iter);
            library->send_object(state_obj);
            if (act_id != worker_id) {
                auto prev_state_obj = library->create_object_with_target(target_func, (worker_id*worker_id + worker_id) / 2 + act_id + 1, size);
                uint8_t * prev_state_shm = reinterpret_cast<uint8_t *> (prev_state_obj->get_value());
                memcpy(prev_state_shm, state_shm, size);
                library->send_object(prev_state_obj);
            }
            
        } catch ( const exception & e ) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
        
    }
}