#include <getopt.h>
#include <iostream>
#include <limits>
#include <cstdlib>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder.hh"
#include "enc_state_serializer.hh"
#include "cpp_function.hpp"

using namespace std;

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        try {
            std::cout << "Test args size " << arg_size << std::endl;
            
            std::string arg (arg_values[0]);
            int worker_id = stoi(arg);

            std::string file_id = std::to_string(worker_id + 1);
            while (file_id.size() < 8) file_id = "0" + file_id;

            std::string input_y4m = "/data/sintel-1k-y4m_06/" + file_id + ".y4m";
            std::cout << input_y4m << endl;

            std::string vpxenc_invocation = "/libvpx-1.10.0/build/vpxenc --ivf -q --codec=vp8 --good --cpu-used=0 --end-usage=cq --min-q=0 --max-q=63 --cq-level=30 --buf-initial-sz=10000 --buf-optimal-sz=20000 --buf-sz=40000 --undershoot-pct=100 --passes=2 --auto-alt-ref=1 --threads=1 --token-parts=0 --tune=ssim --target-bitrate=4294967295 -o /data/ivf/" + std::to_string(worker_id) + "_0.ivf " + input_y4m;

            std::cout << vpxenc_invocation << endl;
            int r = system(vpxenc_invocation.c_str());
            (void) r;

            size_t frame_number = numeric_limits<size_t>::max();
            
            string input_file = "/data/ivf/" + std::to_string(worker_id) + "_0.ivf";
            IVF ivf( input_file );
            Decoder decoder = Decoder( ivf.width(), ivf.height() );

            if ( frame_number == numeric_limits<size_t>::max() ) {
                frame_number = ivf.frame_count() - 1;
            }

            cout << "frame_number " << frame_number << endl;
            string target_func ("xc-enc");
            string ivf_bucket = "bucket" + std::to_string(worker_id);
            string ivf_key = "ivf0";
            int ivf_size = ivf.size();
            if (worker_id != 0) {
                auto ivf_obj = library->create_object_with_target(target_func, (worker_id*worker_id - worker_id) / 2, ivf_size + 2);
                uint8_t * ivf_shm = reinterpret_cast<uint8_t *> (ivf_obj->get_value());
                ivf_shm[0] = (uint8_t) worker_id;
                ivf_shm[1] = (uint8_t) 1;
                memcpy(ivf_shm + 2, ivf.buffer(), ivf_size);
                library->send_object(ivf_obj);
            }
            
            // auto ivf_obj = library->create_object_with_bucket_key(ivf_bucket, ivf_key, ivf_size);
            // uint8_t * ivf_shm = reinterpret_cast<uint8_t *> (ivf_obj->get_value());
            // uint8_t * ivf_shm = reinterpret_cast<uint8_t *> (library->gen_bytes(ivf_bucket, ivf_key, ivf_size));
            // cout << ivf_bucket << " " << ivf_key << " " << ivf_size + 1 << endl;
            
            // memcpy(ivf_shm, ivf.buffer(), ivf_size);
            // for(int i = 0; i < ivf_size; i++) {
            //     cout << (char) ivf_shm[i];
            // }
            // cout << endl;
            // cout << ivf_shm <<" " << ivf_size << endl;
            // bool sync_res = library->sync(ivf_bucket, ivf_key, ivf_size);
            // library->send_object(ivf_obj);
            // cout << sync_res << endl;

            for ( size_t i = 0; i < ivf.frame_count(); i++) {
                UncompressedChunk uch { ivf.frame( i ), ivf.width(), ivf.height(), false };

                if ( uch.key_frame() ) {
                    KeyFrame frame = decoder.parse_frame<KeyFrame>( uch );
                    decoder.decode_frame( frame );
                }
                else {
                    InterFrame frame = decoder.parse_frame<InterFrame>( uch );
                    decoder.decode_frame( frame );
                }

                if ( i == frame_number ) {
                    int size = decoder.get_len();
                    auto state_obj = library->create_object_with_target(target_func, (worker_id*worker_id + worker_id) / 2, size);
                    // string bucket = "bucket" + std::to_string(worker_id + 1);
                    // string key = "state0";
                    
                    // auto state_obj = library->create_object_with_bucket_key(bucket, key, size);
                    uint8_t * state_shm = reinterpret_cast<uint8_t *> (state_obj->get_value());
                    int iter = 0;
                    decoder.serialize_shm(state_shm, iter);
                    // sync_res = library->sync(bucket, key, size);
                    library->send_object(state_obj);

                    if (worker_id != 0) {
                        auto prev_state_obj = library->create_object_with_target(target_func, (worker_id*worker_id + worker_id) / 2 + 1, size);
                        uint8_t * prev_state_shm = reinterpret_cast<uint8_t *> (prev_state_obj->get_value());
                        memcpy(prev_state_shm, state_shm, size);
                        library->send_object(prev_state_obj);
                    }
                    // cout << bucket << " " << key << " " << size << endl;

                    // size = 4;
                    // bucket = "bucket" + std::to_string(worker_id);
                    // key = "worker_id";
                    // auto worker_obj = library->create_object_with_bucket_key(bucket, key, size);
                    // int * worker_id_shm = reinterpret_cast<int *>(worker_obj->get_value());
                    // worker_id_shm[0] = worker_id;
                    // library->send_object(worker_obj);

                    // key = "act_id";
                    // auto act_obj = library->create_object_with_bucket_key(bucket, key, size);
                    // int * act_id_shm = reinterpret_cast<int *>(act_obj->get_value());
                    // act_id_shm[0] = 1;
                    // library->send_object(act_obj);
                    // EncoderStateSerializer odata;
                    // decoder.serialize( odata );
                    
                    return EXIT_SUCCESS;
                }
            }
            throw runtime_error( "invalid frame number" );
        }
        catch ( const exception & e ) {
            print_exception( arg_values[0], e );
            return EXIT_FAILURE;
        }

        return 0;
    }
}
