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
            
            std::string arg {arg_values[0]};
            int worker_id = stoi(arg);

            std::string file_id = std::to_string(worker_id + 1);
            while (file_id.size() < 8) file_id = "0" + file_id;

            std::string input_y4m = "~/data/sintel-1k-y4m_06/" + file_id + ".y4m";
            std::cout << input_y4m << endl;

            std::string vpxenc_invocation = "~/excamera_project/libvpx/build/vpxenc --ivf -q --codec=vp8 --good --cpu-used=0 --end-usage=cq --min-q=0 --max-q=63 --cq-level=30 --buf-initial-sz=10000 --buf-optimal-sz=20000 --buf-sz=40000 --undershoot-pct=100 --passes=2 --auto-alt-ref=1 --threads=1 --token-parts=0 --tune=ssim --target-bitrate=4294967295 -o ~/data/ivf/" + std::to_string(worker_id) + "_0.ivf " + input_y4m;

            std::cout << vpxenc_invocation << endl;
            int r = system(vpxenc_invocation.c_str());
            (void) r;

            size_t frame_number = numeric_limits<size_t>::max();
            
            string input_file = "/home/ubuntu/data/ivf/" + std::to_string(worker_id) + "_0.ivf";
            IVF ivf( input_file );
            Decoder decoder = Decoder( ivf.width(), ivf.height() );

            if ( frame_number == numeric_limits<size_t>::max() ) {
                frame_number = ivf.frame_count() - 1;
            }

            cout << "frame_number " << frame_number << endl;
            string ivf_bucket = "bucket" + std::to_string(worker_id);
            string ivf_key = "ivf0";
            int ivf_size = ivf.size();
            uint8_t * ivf_shm = reinterpret_cast<uint8_t *> (library->gen_bytes(ivf_bucket, ivf_key, ivf_size));
            cout << ivf_bucket << " " << ivf_key << " " << ivf_size + 1 << endl;
            
            memcpy(ivf_shm, ivf.buffer(), ivf_size);
            // for(int i = 0; i < ivf_size; i++) {
            //     cout << (char) ivf_shm[i];
            // }
            // cout << endl;
            // cout << ivf_shm <<" " << ivf_size << endl;
            bool sync_res = library->sync(ivf_bucket, ivf_key, ivf_size);
            cout << sync_res << endl;

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
                    string bucket = "bucket" + std::to_string(worker_id + 1);
                    string key = "state0";
                    int size = decoder.get_len();
                    uint8_t * state_shm = reinterpret_cast<uint8_t *> (library->gen_bytes(bucket, key, size));
                    int iter = 0;
                    decoder.serialize_shm(state_shm, iter);
                    sync_res = library->sync(bucket, key, size);
                    cout << bucket << " " << key << " " << size << endl;

                    size = 4;
                    bucket = "bucket" + std::to_string(worker_id);
                    key = "worker_id";
                    int * worker_id_shm = reinterpret_cast<int *>(library->gen_bytes(bucket, key, size));
                    worker_id_shm[0] = worker_id;

                    key = "act_id";
                    int * act_id_shm = reinterpret_cast<int *>(library->gen_bytes(bucket, key, size));
                    act_id_shm[0] = 1;
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

        return EXIT_SUCCESS;
    }
}
