#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <bcm2835.h>
#include "fmtype.h"
#include "fmsd1.h"
#include "fmif.h"

#define	MIDIDRV_VERSION		"0.1.0"
#define	ALSA_SEQ_CLIENT_NAME	"YMF825 MIDI"
#define	ALSA_SEQ_PORT_NAME	ALSA_SEQ_CLIENT_NAME
#define	ARRAY_SIZE( array )	( (int)( sizeof( array ) / sizeof( (array)[0] ) ) )

typedef	struct	_DRV_PARAM {
	bool	fVerbose;
	bool	fEnableMidiChannel[MAX_MIDI_CHANNEL];
} DRV_PARAM;

volatile int	g_iInterruptReceived = 0;
FILE *		g_fpOut;
DRV_PARAM	g_DrvParam;


static	void	InterruptHandler( int signo )
{
	g_iInterruptReceived = 1;
}


void    DumpReg( void )
{
	unsigned char	bAdrs;
	unsigned char	bData[64];
	int	i;

	memset( bData, 0, sizeof( bData ) );
	for ( i = 0; i < 30; i++ ) {
		bAdrs = (unsigned char)i;
		bData[i] = readSingle( bAdrs );
	}

	fprintf( g_fpOut, "Dump YMF825 Register\n" );
	fprintf( g_fpOut, "     : +0 +1 +2 +3 +4 +5 +6 +7 +8 +9\n" );
	for ( i = 0; i < 30; i++ ) {
		if ( ( i % 10 ) == 0 ) {
			if ( i != 0 ) {
				fprintf( g_fpOut, "\n" );
			}
			fprintf( g_fpOut, "%04d : ", i );
		}
		fprintf( g_fpOut, "%02X ", bData[i] );
	}
	fprintf( g_fpOut, "\n" );
}


int     CommunicationCheck( void )
{
	unsigned char	bWriteData;
	unsigned char	bReadData;
	int	i;

	fprintf( g_fpOut, "YMF825 Communication checking...\n" );

	bWriteData = 0x00;
	for ( i = 0; i < 256; i++ ) {
		writeSingle( 80, bWriteData );
		bReadData = readSingle( 80 );
		if ( bWriteData != bReadData ) {
			fprintf( g_fpOut, "R/W Failed : Write(%02X) Read(%02X)\n", bWriteData, bReadData );
			return -1;
		}
		bWriteData++;
	}

	fprintf( g_fpOut, "YMF825 Communication check complete.\n" );

	return 0;
}


int	CreateSeq( snd_seq_t **seq, int *piClientID, int *piPortID )
{
	snd_seq_client_info_t	*cinfo;
	snd_seq_port_info_t	*pinfo;
	const	char	*pszClientName;
	const	char	*pszPortName;
	int	iPortNo;

	if ( piClientID != NULL ) {
		*piClientID = 0;
	}
	if ( piPortID != NULL ) {
		*piPortID = 0;
	}

	if ( snd_seq_open( seq, "default", SND_SEQ_OPEN_DUPLEX, 0 ) < 0 ) {
		fprintf( stderr, "snd_seq_open fail.\n" );
		return -1;
	}

	snd_seq_set_client_name( *seq, ALSA_SEQ_CLIENT_NAME );

	iPortNo = snd_seq_create_simple_port( *seq, ALSA_SEQ_PORT_NAME,
				SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
				SND_SEQ_PORT_TYPE_MIDI_GENERIC );
	if ( iPortNo < 0 ) {
		fprintf( stderr, "snd_seq_create_simple_port fail.\n" );
		return -1;
	}

	snd_seq_client_info_alloca( &cinfo );
	snd_seq_port_info_alloca( &pinfo );
	snd_seq_client_info_set_client( cinfo, -1 );
	while ( snd_seq_query_next_client( *seq, cinfo) >= 0 ) {
		int iClientID = snd_seq_client_info_get_client( cinfo );

		snd_seq_port_info_set_client( pinfo, iClientID );
		snd_seq_port_info_set_port( pinfo, -1 );
		while ( snd_seq_query_next_port( *seq, pinfo ) >= 0 ) {
			if ( !( snd_seq_port_info_get_type( pinfo ) & SND_SEQ_PORT_TYPE_MIDI_GENERIC ) ) {
				continue;
			}
			if ( ( snd_seq_port_info_get_capability( pinfo )
			    &  ( SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE ) )
			    != ( SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE ) ) {
				continue;
			}
			pszClientName = snd_seq_client_info_get_name( cinfo );
			pszPortName   = snd_seq_port_info_get_name( pinfo );
			if ( strcmp( pszClientName, ALSA_SEQ_CLIENT_NAME ) == 0
			  && strcmp( pszPortName,   ALSA_SEQ_PORT_NAME ) == 0 ) {
				if ( piClientID != NULL ) {
					*piClientID = snd_seq_port_info_get_client( pinfo );
				}
				if ( piPortID != NULL ) {
					*piPortID = snd_seq_port_info_get_port( pinfo );
				}
				break;
			}
		}
	}

	return iPortNo;
}


int	TermSeq( snd_seq_t *seq, int iPortNo )
{
	if ( seq == NULL ) {
		return -1;
	}

	snd_seq_delete_simple_port( seq, iPortNo );

	return snd_seq_close( seq );
}


void	SysExJob( snd_seq_event_t *ev, char *pszTimeInfo )
{
	snd_midi_event_t *sme;
	unsigned char *pbSysExBuf;

	if ( ev->data.ext.len <= 0 ) {
		return;
	}
	pbSysExBuf = malloc( ev->data.ext.len );
	if ( pbSysExBuf == NULL ) {
		return;
	}
	memset( pbSysExBuf, 0, ev->data.ext.len );
	int iResult = snd_midi_event_new( 64, &sme );
	if ( iResult != 0 ) {
		free( pbSysExBuf );
		return;
	}

	snd_midi_event_init( sme );
	long lResult = snd_midi_event_decode( sme, pbSysExBuf, ev->data.ext.len, ev );
	snd_midi_event_free( sme );
	if ( lResult > 0 ) {
		fprintf( g_fpOut, "%s SysEx LEN=%ld : ", pszTimeInfo, lResult );
		for ( long i = 0; i < lResult; i++ ) {
			fprintf( g_fpOut, "%02X ", pbSysExBuf[i] );
			Fmdriver_sendMidi( pbSysExBuf[i] );
		}
		fprintf( g_fpOut, "\n" );
	}

	free( pbSysExBuf );
}


void	DispMidiData( snd_seq_event_type_t seqevType, char *pszTimeInfo, unsigned char *pbMidiBytes )
{
	const struct {
		snd_seq_event_type_t	seqevType;
		const char		*pszMsg;
	} TblMsg[] = {
		//                               123456789012345
		{ SND_SEQ_EVENT_NOTEOFF,	"Note off"        },
		{ SND_SEQ_EVENT_NOTEON,		"Note on"         },
		{ SND_SEQ_EVENT_KEYPRESS,	"Pressure change" },
		{ SND_SEQ_EVENT_CONTROLLER,	"Control change"  },
		{ SND_SEQ_EVENT_PGMCHANGE,	"Program change"  },
		{ SND_SEQ_EVENT_CHANPRESS,	"Channel change"  },
		{ SND_SEQ_EVENT_PITCHBEND,	"Pitch bend"      },
	};

	const char *pszMsg = "unknown";
	for ( int i = 0; i < ARRAY_SIZE( TblMsg ); i++ ) {
		if ( TblMsg[i].seqevType == seqevType ) {
			pszMsg = TblMsg[i].pszMsg;
			break;
		}
	}

	fprintf( g_fpOut, "%s %-15s 0x%02X %3u %3u\n", 
		pszTimeInfo,
		pszMsg,
		pbMidiBytes[0],
		pbMidiBytes[1],
		pbMidiBytes[2] );
}


void	WriteYmf825( snd_seq_t *seq_handle )
{
	struct timeval	NowTimeVal;
	snd_seq_event_t	*ev;
	unsigned char	bMidiBytes[] = { 0x00, 0x00, 0xFF };
	char	szTimeInfo[64];

	do {
		snd_seq_event_input( seq_handle, &ev );

		gettimeofday( &NowTimeVal, NULL );
		sprintf( szTimeInfo, "[%ld.%06ld]", NowTimeVal.tv_sec, NowTimeVal.tv_usec );

		switch ( ev->type ) {
			case SND_SEQ_EVENT_NOTEOFF:
			case SND_SEQ_EVENT_NOTEON:
			case SND_SEQ_EVENT_KEYPRESS:
			case SND_SEQ_EVENT_CONTROLLER:
			case SND_SEQ_EVENT_PGMCHANGE:
			case SND_SEQ_EVENT_CHANPRESS:
			case SND_SEQ_EVENT_PITCHBEND:
				if ( g_DrvParam.fEnableMidiChannel[ev->data.control.channel] == false ) {
					ev->type = SND_SEQ_EVENT_NONE;
				}
				break;
		}

		switch ( ev->type ) {
			case SND_SEQ_EVENT_NOTEOFF:
				bMidiBytes[0] = 0x80 + ev->data.control.channel;
				bMidiBytes[1] = ev->data.note.note;
				bMidiBytes[2] = ev->data.note.velocity;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_NOTEON:
				bMidiBytes[0] = 0x90 + ev->data.control.channel;
				bMidiBytes[1] = ev->data.note.note;
				bMidiBytes[2] = ev->data.note.velocity;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_KEYPRESS:
				bMidiBytes[0] = 0xA0 + ev->data.control.channel;
				bMidiBytes[1] = ev->data.note.note;
				bMidiBytes[2] = ev->data.note.velocity;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_CONTROLLER:
				bMidiBytes[0] = 0xB0 + ev->data.control.channel;
				bMidiBytes[1] = ev->data.control.param;
				bMidiBytes[2] = ev->data.control.value;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_PGMCHANGE:
				bMidiBytes[0] = 0xC0 + ev->data.control.channel;
				bMidiBytes[1] = ev->data.control.value;
				bMidiBytes[2] = 0xFF;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_CHANPRESS:
				bMidiBytes[0] = 0xD0 + ev->data.control.channel;
				bMidiBytes[1] = ev->data.control.value;
				bMidiBytes[2] = 0xFF;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_PITCHBEND:
				bMidiBytes[0] = 0xE0 + ev->data.control.channel;
				ev->data.control.value += 8192;
				bMidiBytes[1] = (unsigned char)( ev->data.control.value & 0x7F );
				bMidiBytes[2] = (unsigned char)( ev->data.control.value >> 7 );
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;

			case SND_SEQ_EVENT_SYSEX:
				SysExJob( ev, szTimeInfo );
				bMidiBytes[0] = 0x00;
				bMidiBytes[1] = 0x00;
				bMidiBytes[2] = 0xFF;
				break;

			case SND_SEQ_EVENT_PORT_SUBSCRIBED:
			case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
				// All Notes Off
				for ( int i = 0; i < MAX_MIDI_CHANNEL; i++ ) {
					bMidiBytes[0] = 0xB0 | (unsigned char)i;
					bMidiBytes[1] = 120;
					bMidiBytes[2] = 0;
					Fmdriver_sendMidi( bMidiBytes[0] );
					Fmdriver_sendMidi( bMidiBytes[1] );
					Fmdriver_sendMidi( bMidiBytes[2] );
				}
				bMidiBytes[0] = 0x00;
				bMidiBytes[1] = 0x00;
				bMidiBytes[2] = 0xFF;
				break;

			case SND_SEQ_EVENT_NONE:
				bMidiBytes[0] = 0x00;
				bMidiBytes[1] = 0x00;
				bMidiBytes[2] = 0xFF;
				break;

			default:
				bMidiBytes[0] = 0x00;
				bMidiBytes[1] = 0x00;
				bMidiBytes[2] = 0xFF;
				DispMidiData( ev->type, szTimeInfo, bMidiBytes );
				break;
		}

		if ( bMidiBytes[0] != 0x00 ) {
			bMidiBytes[1] &= 0x7F;
			if ( bMidiBytes[2] == 0xFF ) {
				Fmdriver_sendMidi( bMidiBytes[0] );
				Fmdriver_sendMidi( bMidiBytes[1] );
			} else {
				bMidiBytes[2] &= 0x7F;
				Fmdriver_sendMidi( bMidiBytes[0] );
				Fmdriver_sendMidi( bMidiBytes[1] );
				Fmdriver_sendMidi( bMidiBytes[2] );
			}
		}

		snd_seq_free_event( ev );
	}
	while ( snd_seq_event_input_pending( seq_handle, 0 ) > 0 );
}


void	*ThreadYmf825drv( void *args )
{
	snd_seq_t	*seq_handle;
	struct pollfd	*pfd;
	int	npfd;

	seq_handle = args;

	npfd = snd_seq_poll_descriptors_count( seq_handle, POLLIN );
	pfd = (struct pollfd *)alloca( npfd * sizeof( struct pollfd ) );
	snd_seq_poll_descriptors( seq_handle, pfd, npfd, POLLIN );

	while ( !g_iInterruptReceived ) {
		if ( poll( pfd, npfd, 100 ) > 0 ) {
			WriteYmf825( seq_handle );
		}
	}

	return NULL;
}


bool	SetupEnableMidiChannel( char *pszEnableMidiChannel )
{
	bool	fEnable;
	int	iLen;
	int	i;

	iLen = strlen( optarg );
	for ( i = 0; i < iLen; i++ ) {
		if ( i < 0 || i >= ARRAY_SIZE( g_DrvParam.fEnableMidiChannel ) ) {
			return false;
		}
		if ( optarg[i] == '1' ) {
			fEnable = true;
		}
		else if ( optarg[i] == '0' ) {
			fEnable = false;
		} else {
			return false;
		}
		g_DrvParam.fEnableMidiChannel[i] = fEnable;
		pszEnableMidiChannel[i]          = optarg[i];
	}

	return true;
}


void	version( const char *argv0 )
{
	printf( "%s version %s\n", argv0, MIDIDRV_VERSION );
}


void	usage( const char *argv0 )
{
	printf( 
	"Usage: %s [OPTION...]\n"
	"-h, --help           this help.\n"
	"-V, --version        print current version.\n"
	"-v, --verbose        verbose output.                         default No\n"
	"-d, --dram           Enable MIDI Default Dram Channel(10).   default No\n"
	"-e, --eanble=param   Enable MIDI Channel.                    default 1111111110111111\n"
	"                     MIDI Ch. 0123456789ABCDEF\n"
	"                        param=1111000000000000 Eanble Ch. 0,1,2,3\n"
	"                        param=1000010000000000 Eanble Ch. 0,5\n"
	"                        param=1000000000000000 Eanble Ch. 0\n",
	argv0 );
}


int	main( int argc, char **argv )
{
	const char short_options[] = "hVvde:";
	const struct option long_options[] = {
		{ "help",      0, NULL, 'h' },
		{ "version",   0, NULL, 'V' },
		{ "verbose",   0, NULL, 'v' },
		{ "dram",      0, NULL, 'd' },
		{ "enable",    1, NULL, 'e' },
		{}
	};
	snd_seq_t	*seq;
	pthread_t	midiThread;
	char	szEnableMidiChannel[64];
	int	iPortNo;
	int	iClientID;
	int	iPortID;
	int	c;

	memset( szEnableMidiChannel, 0, sizeof( szEnableMidiChannel ) );
	g_DrvParam.fVerbose = false;
	for ( int i = 0; i < ARRAY_SIZE( g_DrvParam.fEnableMidiChannel ); i++ ) {
		bool	fEnable = true;
		char	cData = '1';
		if ( i == 9 ) {
			fEnable = false;
			cData   = '0';
		}
		g_DrvParam.fEnableMidiChannel[i] = fEnable;
		szEnableMidiChannel[i]           = cData;
	}

	while ( ( c = getopt_long( argc, argv, short_options, long_options, NULL ) ) != -1 ) {
		switch ( c ) {
			case 'h':
				usage( argv[0] );
				return 0;
			case 'V':
				version( argv[0] );
				return 0;
			case 'v':
				g_DrvParam.fVerbose = true;
				break;
			case 'd':
				g_DrvParam.fEnableMidiChannel[9] = true;
				break;
			case 'e':
				if ( SetupEnableMidiChannel( szEnableMidiChannel ) == false ) {
					usage( argv[0] );
					return 1;
				}
				break;
			default:
				usage( argv[0] );
				return 1;
		}
	}

	if ( g_DrvParam.fVerbose == true ) {
		g_fpOut = stdout;
	} else {
		g_fpOut = fopen( "/dev/null", "w" );
	}

	if ( !bcm2835_init() ) {
		fprintf( stderr, "bcm2835_init fail.\n" );
		return 1;
	}

	fprintf( g_fpOut, "Initialize SPI.\n" );
	initSPI();

	fprintf( g_fpOut, "Initialize YMF825.\n" );
	initSD1();

	DumpReg();
	if ( CommunicationCheck() == -1 ) {
		bcm2835_spi_end();
		bcm2835_close();
		return 1;
	}

	fprintf( g_fpOut, "Initialize FM Driver.\n" );
	Fmdriver_init();

	seq = NULL;

	// Create ALSA sequencer port
	iPortNo = CreateSeq( &seq, &iClientID, &iPortID );
	if ( iPortNo < 0 ) {
		bcm2835_spi_end();
		bcm2835_close();
		return 1;
	}

	fprintf( g_fpOut, "YMF825 MIDI Driver Version %s ready.\n", MIDIDRV_VERSION );
	fprintf( g_fpOut, "ALSA MIDI Port(%d:%d) '%s' ready.\n", iClientID, iPortID, ALSA_SEQ_CLIENT_NAME );
	fprintf( g_fpOut, "Enable MIDI Channel. 0123456789ABCDEF\n" );
	fprintf( g_fpOut, "                     %s\n", szEnableMidiChannel );

	signal( SIGTERM, InterruptHandler );
	signal( SIGINT,  InterruptHandler );

	// Start thread
	pthread_create( &midiThread, NULL, ThreadYmf825drv, (void *)seq );

	while ( !g_iInterruptReceived ) {
		// Wait for signal
		pause();
		break;
	}

	// Cancel thread
	pthread_cancel( midiThread );
	// Wait thread
	pthread_join( midiThread, NULL );

	// Terminate ALSA sequencer port
	TermSeq( seq, iPortNo );

	bcm2835_spi_end();
	bcm2835_close();

	return 0;
}
