// fmif.c
#include	"fmif.h"
#include	"fmpart.h"

//	Variable
static Part		_part[MAX_MIDI_CHANNEL];
static unsigned char	_midiCmdCounter;
static unsigned char	_midiStatus;
static unsigned char	_midiDataByte1;
static unsigned char	_midiDataByte2;

//	Prototype
static void receiveDataByte( unsigned char byteStream );
static void	generateMidiCmd(void);

void Fmdriver_init( void )
{
	_midiCmdCounter = 0;
	_midiStatus = 0;
	_midiDataByte1 = 0;
	_midiDataByte2 = 0;

	Asgn_init();
	Tone_init();

	for ( int i = 0; i < MAX_MIDI_CHANNEL; i++ ) {
		Part_init( &_part[i] );
		Part_pc( &_part[i], 0 );
	}
}
void Fmdriver_sendMidi( unsigned char byteStream )
{
	if ( byteStream & 0x80 ){
		if ( byteStream == 0xf7 ){
			Tone_sendTone();
			_midiStatus = 0;
			_midiCmdCounter = 0;
		}
		else {
			_midiStatus = byteStream;
			_midiCmdCounter = 1;
		}
	}
	else if ( _midiStatus == 0xf0 ){
		Tone_setToneExc(byteStream,_midiCmdCounter);
		_midiCmdCounter += 1;
	}
	else if ( _midiStatus != 0 ){
		receiveDataByte(byteStream);
	}
}
static void receiveDataByte( unsigned char byteStream )
{
	switch (_midiCmdCounter){
		case 0: case 1:{
			_midiDataByte1 = byteStream;
			switch ( _midiStatus & 0xf0 ){
				case 0xc0: case 0xd0:{
					_midiCmdCounter = 0;
					generateMidiCmd();
					break;
				}
				default:{
					_midiCmdCounter = 2;
					break;
				}
			}
			break;
		}
		case 2:{
			_midiDataByte2 = byteStream;
			_midiCmdCounter = 0;
			generateMidiCmd();
			break;
		}
		default: break;
	}
}
static void generateMidiCmd( void )
{
	unsigned char	bMidiChNo;
	unsigned char	bMidiStatus;

	bMidiStatus = _midiStatus & 0xF0;
	bMidiChNo   = _midiStatus & 0x0F;

	switch ( bMidiStatus ){
		case 0x80: Part_note( &_part[bMidiChNo], _midiDataByte1, 0 ); break;
		case 0x90: Part_note( &_part[bMidiChNo], _midiDataByte1, _midiDataByte2 ); break;
		case 0xb0: Part_cc( &_part[bMidiChNo], _midiDataByte1, _midiDataByte2 ); break;
		case 0xc0: Part_pc( &_part[bMidiChNo], _midiDataByte1 ); break;
		case 0xe0: Part_pbend( &_part[bMidiChNo], _midiDataByte1, _midiDataByte2 ); break;
		default: break;
	}
}
