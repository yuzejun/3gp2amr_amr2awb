/*====================================================================================
    EVS Codec 3GPP TS26.443 Aug 30, 2016. Version 12.7.0 / 13.3.0
  ====================================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "options.h"
#include "prot.h"
#include "cnst.h"
#include "rom_com.h"
#include "g192.h"

/*------------------------------------------------------------------------------------------*
 * Global variables
 *------------------------------------------------------------------------------------------*/
long frame = 0;                 /* Counter of frames */





/*------------------------------------------------------------------------------------------*
 * Main decoder function
 *------------------------------------------------------------------------------------------*/

int main( int argc, char *argv[] )
{
    FILE *f_stream;                     /* input bitstream file */
    FILE *f_synth;                      /* output synthesis file */
    Decoder_State dst;
    Decoder_State *st;                  /* decoder state structure */
    short output_frame, dec_delay, zero_pad;
    short quietMode = 0;
    //short noDelayCmp = 0;
    float output[L_FRAME48k];           /* 'float' buffer for output synthesis */
    short data[L_FRAME48k];             /* 'short' buffer for output synthesis */
#ifdef SUPPORT_JBM_TRACEFILE
    char *jbmTraceFileName = NULL;      /* VOIP tracefile name */
#endif
    char *jbmFECoffsetFileName = NULL;  /* VOIP tracefile name */





    /*------------------------------------------------------------------------------------------*
     * Allocate memory for static variables
     * Processing of command-line parameters
     * Decoder initialization
     *------------------------------------------------------------------------------------------*/

    st = &dst;

// disable input file detect, only support mime and 8000

//    io_ini_dec( argc, argv, &f_stream, &f_synth,
//                &quietMode, &noDelayCmp, st,
//#ifdef SUPPORT_JBM_TRACEFILE
//                &jbmTraceFileName,
//#endif
//                &jbmFECoffsetFileName
//              );

     st->Opt_AMR_WB = 0;
     st->Opt_VOIP = 0;
     st->bitstreamformat = MIME;
     st->amrwb_rfc4867_flag = 0;
     f_stream = stdin;
     char buf[1024];
     fgets(buf, 1024, f_stream);
     fread(buf, sizeof(char), 4, f_stream);
     f_synth = stdout;

     st->output_Fs = 8000;

    /*------------------------------------------------------------------------------------------*
     * VOIP client
     *------------------------------------------------------------------------------------------*/

    if( st->Opt_VOIP )
    {
        if( decodeVoip( st, f_stream, f_synth,
#ifdef SUPPORT_JBM_TRACEFILE
                        jbmTraceFileName,
#endif
                        jbmFECoffsetFileName,
                        quietMode
                      ) != 0 )
        {
            fclose( f_synth );
            fclose( f_stream );
            return -1;
        }
    }

    /*------------------------------------------------------------------------------------------*
     * Regular EVS decoder with ITU-T G.192 bitstream
     *------------------------------------------------------------------------------------------*/

    else
    {
        /*------------------------------------------------------------------------------------------*
         * Allocate memory for static variables
         * Decoder initialization
         *------------------------------------------------------------------------------------------*/

        init_decoder( st );
        reset_indices_dec( st );

        srand( (unsigned int) time(0) );

        /* output frame length */
        output_frame = (short)(st->output_Fs / 50);

        // always turn on no_delay_compensation
        dec_delay = 0;
        //if( noDelayCmp == 0 )
        //{
        //    /* calculate the delay compensation to have the decoded signal aligned with the original input signal */
        //    /* the number of first output samples will be reduced by this amount */
        //    dec_delay = NS2SA(st->output_Fs, get_delay(DEC, st->output_Fs));
        //}
        //else
        //{
        //    dec_delay = 0;
        //}
        zero_pad = dec_delay;

        /*------------------------------------------------------------------------------------------*
         * Loop for every packet (frame) of bitstream data
         * - Read the bitstream packet
         * - Run the decoder
         * - Write the synthesized signal into output file
         *------------------------------------------------------------------------------------------*/

        //if( quietMode == 0 )
        //{
        //    fprintf( stderr, "\n------ Running the decoder ------\n\n" );
        //    fprintf( stderr, "Frames processed:       " );
        //}
        //else
        //{
        //    fprintf( stderr, "\n-- Start the decoder (quiet mode) --\n\n" );
        //}


        // always read mime, and split reader and decoder
        //while( st->bitstreamformat==G192 ? read_indices( st, f_stream, 0 ) : read_indices_mime( st, f_stream, 0) )
        for(;;)
        {
            UWord8 header;
            UWord8 frame_body[(MAX_BITS_PER_FRAME + 7) >> 3];

            if (!read_frame(f_stream, &header, frame_body)) {
                break;
            }
            update_state(st, header, frame_body);
            /* run the main decoding routine */
            if ( st->codec_mode == MODE1 )
            {
                if ( st->Opt_AMR_WB )
                {
                    amr_wb_dec( st, output );
                }
                else
                {
                    evs_dec( st, output, FRAMEMODE_NORMAL );
                }
            }
            else
            {
                if( !st->bfi )
                {
                    evs_dec( st, output, FRAMEMODE_NORMAL );
                }
                else
                {
                    evs_dec( st, output, FRAMEMODE_MISSING );
                }
            }

            /* convert 'float' output data to 'short' */
            syn_output( output, output_frame, data );
            /* increase the counter of initialization frames */
            if( st->ini_frame < MAX_FRAME_COUNTER )
            {
                st->ini_frame++;
            }

            /* write the synthesized signal into output file */
            /* do final delay compensation */
            if ( dec_delay == 0 )
            {
                fwrite( data, sizeof(short), output_frame, f_synth );
                fflush(f_synth);
            }
            else
            {
                if ( dec_delay <= output_frame )
                {
                    fwrite( &data[dec_delay], sizeof(short), output_frame - dec_delay, f_synth );
                    dec_delay = 0;
                    fflush(f_synth);
                }
                else
                {
                    dec_delay -= output_frame;
                }
            }

            frame++;
            if( quietMode == 0 )
            {
                //fprintf( stderr, "%-8ld\b\b\b\b\b\b\b\b", frame );
            }



        }


        fflush( stderr );
        //if (quietMode == 0)
        //{
        //    fprintf( stderr, "\n\n" );
        //    fprintf(stderr, "Decoding finished\n\n");
        //}
        //else
        //{
        //    fprintf(stderr, "Decoding of %ld frames finished\n\n", frame);
        //}
        //fprintf( stderr, "\n\n" );


        /* add zeros at the end to have equal length of synthesized signals */
        set_s( data, 0, zero_pad );
        fwrite( data, sizeof(short), zero_pad, f_synth );
        fflush(f_synth);
    }


    fclose( f_synth );
    fclose( f_stream );

    return 0;
}

