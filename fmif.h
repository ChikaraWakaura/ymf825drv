#ifndef FMIF_H
#define FMIF_H

#define	MAX_MIDI_CHANNEL	16

//	public
extern void Fmdriver_init( void );
extern void Fmdriver_sendMidi( unsigned char byteStream );
#endif
