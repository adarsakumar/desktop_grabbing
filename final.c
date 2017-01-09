/*****************************************************************************************************
Developer	 : Adarsa Kumar M
Email		 : aadarsakumar.m@gmail.com
Platform 	 : Redhat linux 6.5
Compiler 	 : gcc
Developed on : 06-01-2017
External Libs: x11,xcb
Version		 : 1.0.0
Description	 : This program grab video from the desktop and encode the video into mpg file 
			   format(record.mpg) on the current directory. 
			   Also the kbhit function is implemented to stop recording.
Compilation	 : gcc -o final final.c -lavdevice -lavcodec -lavformat -lavutil
******************************************************************************************************/
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/*Implementation of kbhit function in linux*/
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf; 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK); 
  ch = getchar(); 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf); 
  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  } 
  return 0;
}
/* The main function for grabbing video*/
int main(int argc, char *argv[])
{
	AVFormatContext *pFormatCtx = NULL;
	AVCodecContext *pCodecCtx,*c= NULL;
    AVCodec *pCodec,*codec;
    AVFrame *pFrame,*pFrameOut;
	AVPacket packet,pkt;
	struct SwsContext * pSwsCtx;
    AVDictionary * p_options = NULL;
    AVInputFormat * p_in_fmt = NULL;
	int codec_id= AV_CODEC_ID_MPEG1VIDEO;
    int frameFinished;
    int i, videoStream;
	int ret, got_output; 
	int crop_w=1366,crop_h=768;	//croping screen width and height 
	/* If don't want to use croping give actual width and height */
	FILE *f;
	char filename[12]="record.mpg";
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };	
	// Register all formats and codecs
    av_register_all();
    avcodec_register_all();
    avdevice_register_all();	
	av_dict_set(&p_options, "framerate", "25", 0);
	//Specify the screen size to be capture
    av_dict_set(&p_options, "video_size", "1366x768", 0);
    p_in_fmt = av_find_input_format("x11grab");
    // Open video input
    if (avformat_open_input(&pFormatCtx, ":0.0",p_in_fmt, &p_options) != 0)
    {
       	printf("cannot open input device!\n");
       	return -1; // Couldn't open file
    }
    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
       	return -1; // Couldn't find stream information
    // Find the first video stream
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type== AVMEDIA_TYPE_VIDEO)
		{
        	videoStream = i;
       		 break;
    	}
    if (videoStream == -1)
       	return -1; // Didn't find a video stream	
   	// Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;

   	// Find the decoder for the video stream
   	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
       	fprintf(stderr, "Unsupported codec!\n");
       	return -1; // Codec not found
    }
    // Open codec for decoding
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
       	return -1; // Could not open codec

	/* find the video encoder */
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
       	fprintf(stderr, "Codec not found\n");
       	exit(1);
    }
   	c = avcodec_alloc_context3(codec);
    if (!c) {
       	fprintf(stderr, "Could not allocate video codec context\n");
       	exit(1);
    }
    // Allocate video frame
    pFrame = av_frame_alloc();      	
    /* put sample parameters for encoder*/
    c->bit_rate = 400000;
    /* resolution must be a multiple of two *///1024x768
    c->width = crop_w;
    c->height = crop_h;
    /* frames per second */
    c->time_base = (AVRational){1,25};
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
	/* Open codec for encoding */
	if (avcodec_open2(c, codec, NULL) < 0) {
       	fprintf(stderr, "Could not open codec\n");
       	exit(1);
    }
	pSwsCtx = sws_getContext(crop_w, crop_h,pCodecCtx->pix_fmt,crop_w, crop_h, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	pFrameOut = av_frame_alloc();
	pFrameOut->format = c->pix_fmt;
    pFrameOut->width  = c->width;
    pFrameOut->height = c->height;	

	ret = av_image_alloc(pFrameOut->data, pFrameOut->linesize, c->width, c->height,c->pix_fmt, 32);
    if (ret < 0) {
       	fprintf(stderr, "Could not allocate raw picture buffer\n");
       	exit(1);
    }
	/* Open the output file */
	f = fopen(filename, "wb");
	if (!f) {
       	fprintf(stderr, "Could not open test.mpg\n" );
       	exit(1);
    }
	//Initializing the packet
	av_init_packet(&pkt); 
	// Read frames and save to disk
    i = 0;	
    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream)
        {   
			// Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            if (frameFinished)
            {
                // Encoding for video 
				sws_scale(pSwsCtx,pFrame->data,pFrame->linesize, 0, crop_h,pFrameOut->data, pFrameOut->linesize);
				pFrameOut->pts = i;
				ret = avcodec_encode_video2(c, &pkt, pFrameOut, &got_output);	
				if (ret < 0) {
            		fprintf(stderr, "Error encoding frame\n");
            		exit(1);
				}	
				if (got_output) {
					printf("Write frame %3d (size=%5d)\n", i, pkt.size);
					fwrite(pkt.data, 1, pkt.size, f);          
				}
				i++;	
				//To stop
				if(kbhit())
					break;	
			}		
		}
    	// Free the packet that was allocated by av_read_frame
    	av_free_packet(&packet);
		av_free_packet(&pkt);
    }
	/* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);  
	// Free the YUV frame
    av_free(pFrame);
	av_free(pFrameOut);
    // Close the codecs
    avcodec_close(pCodecCtx);
	avcodec_close(c);
    // Close the video device
    avformat_close_input(&pFormatCtx);	
    return 0;
}
