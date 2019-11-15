#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>

#ifdef __linux__
    #include <sys/time.h>
#endif

// comment the next line to busy-wait at each frame
//#define __SLEEP__
#define FRAME  16667
#define PERIOD 1000000
#define NAME   "screencap"
#define NAMESP "         "
#define BPP    4

#define SX 0
#define SY 150
#define SW 1680
#define SH 1050

struct shmimage
{
    XShmSegmentInfo shminfo ;
    XImage * ximage ;
    unsigned int * data ; // will point to the image's BGRA packed pixels
} ;

void initimage( struct shmimage * image )
{
    image->ximage = NULL ;
    image->shminfo.shmaddr = (char *) -1 ;
}

void destroyimage( Display * dsp, struct shmimage * image )
{
    if( image->ximage )
    {
        XShmDetach( dsp, &image->shminfo ) ;
        XDestroyImage( image->ximage ) ;
        image->ximage = NULL ;
    }

    if( image->shminfo.shmaddr != ( char * ) -1 )
    {
        shmdt( image->shminfo.shmaddr ) ;
        image->shminfo.shmaddr = ( char * ) -1 ;
    }
}

int createimage( Display * dsp, struct shmimage * image, int width, int height )
{
    // Create a shared memory area 
    image->shminfo.shmid = shmget( IPC_PRIVATE, width * height * BPP, IPC_CREAT | 0600 ) ;
    if( image->shminfo.shmid == -1 )
    {
        perror( NAME ) ;
        return false ;
    }

    // Map the shared memory segment into the address space of this process
    image->shminfo.shmaddr = (char *) shmat( image->shminfo.shmid, 0, 0 ) ;
    if( image->shminfo.shmaddr == (char *) -1 )
    {
        perror( NAME ) ;
        return false ;
    }

    image->data = (unsigned int*) image->shminfo.shmaddr ;
    image->shminfo.readOnly = false ;

    // Mark the shared memory segment for removal
    // It will be removed even if this program crashes
    shmctl( image->shminfo.shmid, IPC_RMID, 0 ) ;

    // Allocate the memory needed for the XImage structure
    image->ximage = XShmCreateImage( dsp, XDefaultVisual( dsp, XDefaultScreen( dsp ) ),
                        DefaultDepth( dsp, XDefaultScreen( dsp ) ), ZPixmap, 0,
                        &image->shminfo, 0, 0 ) ;
    if( !image->ximage )
    {
        destroyimage( dsp, image ) ;
        printf( NAME ": could not allocate the XImage structure\n" ) ;
        return false ;
    }

    image->ximage->data = (char *)image->data ;
    image->ximage->width = width ;
    image->ximage->height = height ;

    // Ask the X server to attach the shared memory segment and sync
    XShmAttach( dsp, &image->shminfo ) ;
    XSync( dsp, false ) ;
    return true ;
}

void getrootwindow( Display * dsp, struct shmimage * image )
{
    XShmGetImage( dsp, XDefaultRootWindow( dsp ), image->ximage, 0, 0, AllPlanes ) ;
}

long timestamp( )
{
   struct timeval tv ;
   struct timezone tz ;
   gettimeofday( &tv, &tz ) ;
   return tv.tv_sec*1000000L + tv.tv_usec ;
}

unsigned int getpixel( struct shmimage * src, 
                       int j, int i, int w, int h )
{
    int x = (float)(i * src->ximage->width) / (float)w ;
    int y = (float)(j * src->ximage->height) / (float)h ;
    return src->data[ y * src->ximage->width + x ] ;
}

int processimage( struct shmimage * src, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    int sw = src->ximage->width ;
    int sh = src->ximage->height ;

    float r = 0;
    float g = 0;
    float b = 0;

    int j, i ;
    for( j = SY ; j < (SY + SH) ; ++j )
    {
        for( i = SX ; i < (SX + SW) ; ++i )
        {
            int d = getpixel( src, j, i, sw, sh ) ;
            uint8_t rr = d >> 16;
            uint8_t gg = d >> 8;
            uint8_t bb = d;
            r += rr;
            g += gg;
            b += bb;
        }
    }

    int npix = SW * SH;
    *red = r / npix;
    *green = g / npix;
    *blue = b / npix;
    return true ;
}

int run( Display * dsp, struct shmimage * src, char *addr)
{
    XGCValues xgcvalues ;
    xgcvalues.graphics_exposures = False ;

    XEvent xevent ;
    int running = true ;
    int initialized = false ;
    long framets = timestamp( ) ;
    long periodts = timestamp( ) ;
    long frames = 0 ;
    int fd = ConnectionNumber( dsp ) ;

    struct sockaddr_in servaddr;
    struct hostent *address;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("Unable to create UDP socket\n");
        return false;
    }

    address = gethostbyname(addr);

    if (address == NULL) {
        switch (h_errno) {
            case HOST_NOT_FOUND:
                fprintf(stderr, "Host not found: %s\n", addr);
                return -1;
            case NO_DATA:
                fprintf(stderr, "No address: %s\n", addr);
                return -1;
            case NO_RECOVERY:
                fprintf(stderr, "Unrecoverable network error\n");
                return -1;
            case TRY_AGAIN:
                fprintf(stderr, "Can't look up address: try again.\n");
                return -1;
        }
    }
             
    

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ((struct in_addr *)address->h_addr_list[0])->s_addr;
    servaddr.sin_port = htons(6502);
    
    while( running )
    {
            getrootwindow( dsp, src ) ;
            uint8_t r, g, b;
            if( !processimage( src, &r, &g, &b ) )
            {
                return false ;
            }
            uint8_t buf[4] = {
                r, g, b, 0 
            };
            sendto(sockfd, buf, 4, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    }
    return true ;
}

int main( int argc, char * argv[] )
{
    if (argc != 2) {
        fprintf(stderr, "Usage: ambientlight <hostname>\n");
        return 10;
    }
    Display * dsp = XOpenDisplay( NULL ) ;
    if( !dsp )
    {
        printf( NAME ": could not open a connection to the X server\n" ) ;
        return 1 ;
    }

    if( !XShmQueryExtension( dsp ) )
    {
        XCloseDisplay( dsp ) ;
        printf( NAME ": the X server does not support the XSHM extension\n" ) ;
        return 1 ;
    }

    int screen = XDefaultScreen( dsp ) ;
    struct shmimage src ;
    initimage( &src ) ;
    int width = XDisplayWidth( dsp, screen ) ;
    int height = XDisplayHeight( dsp, screen ) ;

    if( !createimage( dsp, &src, width, height ) )
    {
        XCloseDisplay( dsp ) ;
        return 1 ;
    }

    run( dsp, &src, argv[1] ) ;

    destroyimage( dsp, &src ) ;
    XCloseDisplay( dsp ) ;
    return 0 ;
}
