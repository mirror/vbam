#ifndef NO_XAUDIO2

// MFC
#include "stdafx.h"

// Application
#include "VBA.h"

// Interface
#include "../common/SoundDriver.h"

// XAudio2
#include <xaudio2.h>

// MMDevice API
#include <mmdeviceapi.h>
#include <vector>
#include <string>

// Internals
#include "../System.h" // for systemMessage()
#include "../gba/Globals.h"


class XAudio2_Output;

static void xaudio2_device_changed( XAudio2_Output * );

class XAudio2_Device_Notifier : public IMMNotificationClient
{
	volatile LONG registered;
	IMMDeviceEnumerator *pEnumerator;

	std::wstring last_device;

	CRITICAL_SECTION lock;
	std::vector<XAudio2_Output*> instances;

public:
	XAudio2_Device_Notifier() : registered( 0 )
	{
		InitializeCriticalSection( &lock );
	}
	~XAudio2_Device_Notifier()
	{
		DeleteCriticalSection( &lock );
	}

	ULONG STDMETHODCALLTYPE AddRef()
	{
		return 1;
	}

	ULONG STDMETHODCALLTYPE Release()
	{
		return 1;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, VOID **ppvInterface )
	{
		if (IID_IUnknown == riid)
		{
			*ppvInterface = (IUnknown*)this;
		}
		else if (__uuidof(IMMNotificationClient) == riid)
		{
			*ppvInterface = (IMMNotificationClient*)this;
		}
		else
		{
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged( EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId )
	{
		if ( flow == eRender && last_device.compare( pwstrDeviceId ) != 0 )
		{
			last_device = pwstrDeviceId;

			EnterCriticalSection( &lock );
			for ( auto it = instances.begin(); it < instances.end(); ++it )
			{
				xaudio2_device_changed( *it );
			}
			LeaveCriticalSection( &lock );
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceAdded( LPCWSTR pwstrDeviceId ) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnDeviceRemoved( LPCWSTR pwstrDeviceId ) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged( LPCWSTR pwstrDeviceId, DWORD dwNewState ) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged( LPCWSTR pwstrDeviceId, const PROPERTYKEY key ) { return S_OK; }

	void do_register(XAudio2_Output * p_instance)
	{
		if ( InterlockedIncrement( &registered ) == 1 )
		{
			pEnumerator = NULL;
			HRESULT hr = CoCreateInstance( __uuidof( MMDeviceEnumerator ), NULL, CLSCTX_INPROC_SERVER, __uuidof( IMMDeviceEnumerator ), ( void** ) &pEnumerator );
			if ( SUCCEEDED( hr ) )
			{
				pEnumerator->RegisterEndpointNotificationCallback( this );
			}
		}

		EnterCriticalSection( &lock );
		instances.push_back( p_instance );
		LeaveCriticalSection( &lock );
	}

	void do_unregister( XAudio2_Output * p_instance )
	{
		if ( InterlockedDecrement( &registered ) == 0 )
		{
			if (pEnumerator)
			{
				pEnumerator->UnregisterEndpointNotificationCallback( this );
				pEnumerator->Release();
				pEnumerator = NULL;
			}
		}

		EnterCriticalSection( &lock );
		for ( auto it = instances.begin(); it < instances.end(); ++it )
		{
			if ( *it == p_instance )
			{
				instances.erase( it );
				break;
			}
		}
		LeaveCriticalSection( &lock );
	}
} g_notifier;

// Synchronization Event
class XAudio2_BufferNotify : public IXAudio2VoiceCallback
{
public:
	HANDLE hBufferEndEvent;

	XAudio2_BufferNotify() {
		hBufferEndEvent = NULL;
		hBufferEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		ASSERT( hBufferEndEvent != NULL );
	}

	~XAudio2_BufferNotify() {
		CloseHandle( hBufferEndEvent );
		hBufferEndEvent = NULL;
	}

    STDMETHOD_( void, OnBufferEnd ) ( void *pBufferContext ) {
		ASSERT( hBufferEndEvent != NULL );
		SetEvent( hBufferEndEvent );
	}


	// dummies:
	STDMETHOD_( void, OnVoiceProcessingPassStart ) ( UINT32 BytesRequired ) {}
	STDMETHOD_( void, OnVoiceProcessingPassEnd ) () {}
	STDMETHOD_( void, OnStreamEnd ) () {}
	STDMETHOD_( void, OnBufferStart ) ( void *pBufferContext ) {}
	STDMETHOD_( void, OnLoopEnd ) ( void *pBufferContext ) {}
	STDMETHOD_( void, OnVoiceError ) ( void *pBufferContext, HRESULT Error ) {};
};


// Class Declaration
class XAudio2_Output
	: public SoundDriver
{
public:
	XAudio2_Output();
	~XAudio2_Output();

	// Initialization
	bool init(long sampleRate);

	// Sound Data Feed
	void write(u16 * finalWave, int length);

	// Play Control
	void pause();
	void resume();
	void reset();
	void close();
	void device_change();

	// Configuration Changes
	void setThrottle( unsigned short throttle );

private:
	bool   failed;
	bool   initialized;
	bool   playing;
	UINT32 freq;
	UINT32 bufferCount;
	BYTE  *buffers;
	int    currentBuffer;
	int    soundBufferLen;

	volatile bool device_changed;

	IXAudio2               *xaud;
	IXAudio2MasteringVoice *mVoice; // listener
	IXAudio2SourceVoice    *sVoice; // sound source
	XAUDIO2_BUFFER          buf;
	XAUDIO2_VOICE_STATE     vState;
	XAudio2_BufferNotify    notify; // buffer end notification
};


// Class Implementation
XAudio2_Output::XAudio2_Output()
{
	failed = false;
	initialized = false;
	playing = false;
	freq = 0;
	bufferCount = theApp.xa2BufferCount;
	buffers = NULL;
	currentBuffer = 0;
	device_changed = false;

	xaud = NULL;
	mVoice = NULL;
	sVoice = NULL;
	ZeroMemory( &buf, sizeof( buf ) );
	ZeroMemory( &vState, sizeof( vState ) );

	g_notifier.do_register( this );
}


XAudio2_Output::~XAudio2_Output()
{
	g_notifier.do_unregister( this );
	close();
}

void XAudio2_Output::close()
{
	initialized = false;

	if( sVoice ) {
		if( playing ) {
			HRESULT hr = sVoice->Stop( 0 );
			ASSERT( hr == S_OK );
		}
		sVoice->DestroyVoice();
		sVoice = NULL;
	}

	if( buffers ) {
		free( buffers );
		buffers = NULL;
	}

	if( mVoice ) {
		mVoice->DestroyVoice();
		mVoice = NULL;
	}

	if( xaud ) {
		xaud->Release();
		xaud = NULL;
	}
}

void XAudio2_Output::device_change()
{
	device_changed = true;
}


bool XAudio2_Output::init(long sampleRate)
{
	if( failed || initialized ) return false;

	HRESULT hr;

	// Initialize XAudio2
	UINT32 flags = 0;
//#ifdef _DEBUG
//	flags = XAUDIO2_DEBUG_ENGINE;
//#endif

	hr = XAudio2Create( &xaud, flags );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_FAILURE, NULL );
		failed = true;
		return false;
	}


	freq = sampleRate;

	// calculate the number of samples per frame first
	// then multiply it with the size of a sample frame (16 bit * stereo)
	soundBufferLen = ( freq / 60 ) * 4;

	// create own buffers to store sound data because it must not be
	// manipulated while the voice plays from it
	buffers = (BYTE *)malloc( ( bufferCount + 1 ) * soundBufferLen );
	// + 1 because we need one temporary buffer when all others are in use

	WAVEFORMATEX wfx;
	ZeroMemory( &wfx, sizeof( wfx ) );
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = freq;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.nChannels * ( wfx.wBitsPerSample / 8 );
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;


	// create sound receiver
	hr = xaud->CreateMasteringVoice(
		&mVoice,
		XAUDIO2_DEFAULT_CHANNELS,
		XAUDIO2_DEFAULT_SAMPLERATE,
		0,
		theApp.xa2Device,
		NULL );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_CANNOT_CREATE_MASTERINGVOICE, NULL );
		failed = true;
		return false;
	}


	// create sound emitter
	hr = xaud->CreateSourceVoice( &sVoice, &wfx, 0, 4.0f, &notify );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_CANNOT_CREATE_SOURCEVOICE, NULL );
		failed = true;
		return false;
	}


	if( theApp.xa2Upmixing ) {
		// set up stereo upmixing
		XAUDIO2_DEVICE_DETAILS dd;
		ZeroMemory( &dd, sizeof( dd ) );
		hr = xaud->GetDeviceDetails( 0, &dd );
		ASSERT( hr == S_OK );
		float *matrix = NULL;
		matrix = (float*)malloc( sizeof( float ) * 2 * dd.OutputFormat.Format.nChannels );
        if( matrix == NULL ) return false;
		bool matrixAvailable = true;
		switch( dd.OutputFormat.Format.nChannels ) {
			case 4: // 4.0
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Back  L*/	matrix[4] = 1.0000f;  matrix[5] = 0.0000f;
	/*Back  R*/	matrix[6] = 0.0000f;  matrix[7] = 1.0000f;
				break;
			case 5: // 5.0
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*Side  L*/	matrix[6] = 1.0000f;  matrix[7] = 0.0000f;
	/*Side  R*/	matrix[8] = 0.0000f;  matrix[9] = 1.0000f;
				break;
			case 6: // 5.1
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*LFE    */	matrix[6] = 0.0000f;  matrix[7] = 0.0000f;
	/*Side  L*/	matrix[8] = 1.0000f;  matrix[9] = 0.0000f;
	/*Side  R*/	matrix[10] = 0.0000f;  matrix[11] = 1.0000f;
				break;
			case 7: // 6.1
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*LFE    */	matrix[6] = 0.0000f;  matrix[7] = 0.0000f;
	/*Side  L*/	matrix[8] = 1.0000f;  matrix[9] = 0.0000f;
	/*Side  R*/	matrix[10] = 0.0000f;  matrix[11] = 1.0000f;
	/*Back  C*/	matrix[12] = 0.7071f;  matrix[13] = 0.7071f;
				break;
			case 8: // 7.1
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*LFE    */	matrix[6] = 0.0000f;  matrix[7] = 0.0000f;
	/*Back  L*/	matrix[8] = 1.0000f;  matrix[9] = 0.0000f;
	/*Back  R*/	matrix[10] = 0.0000f;  matrix[11] = 1.0000f;
	/*Side  L*/	matrix[12] = 1.0000f;  matrix[13] = 0.0000f;
	/*Side  R*/	matrix[14] = 0.0000f;  matrix[15] = 1.0000f;
				break;
			default:
				matrixAvailable = false;
				break;
		}
		if( matrixAvailable ) {
			hr = sVoice->SetOutputMatrix( NULL, 2, dd.OutputFormat.Format.nChannels, matrix );
			ASSERT( hr == S_OK );
		}
		free( matrix );
		matrix = NULL;
	}


	hr = sVoice->Start( 0 );
	ASSERT( hr == S_OK );
	playing = true;

	currentBuffer = 0;
	device_changed = false;

	initialized = true;
	return true;
}


void XAudio2_Output::write(u16 * finalWave, int length)
{
	if( !initialized || failed ) return;

	while( true ) {
		if ( device_changed ) {
			close();
			if (!init(freq)) return;
		}

		sVoice->GetState( &vState );

		ASSERT( vState.BuffersQueued <= bufferCount );

		if( vState.BuffersQueued < bufferCount ) {
			if( vState.BuffersQueued == 0 ) {
				// buffers ran dry
				if( systemVerbose & VERBOSE_SOUNDOUTPUT ) {
					static unsigned int i = 0;
					log( "XAudio2: Buffers were not refilled fast enough (i=%i)\n", i++ );
				}
			}
			// there is at least one free buffer
			break;
		} else {
			// the maximum number of buffers is currently queued
			if( synchronize && !speedup && !theApp.throttle && !gba_joybus_active ) {
				// wait for one buffer to finish playing
				if (WaitForSingleObject( notify.hBufferEndEvent, 10000 ) == WAIT_TIMEOUT) {
					device_changed = true;
				}
			} else {
				// drop current audio frame
				return;
			}
		}
	}

	// copy & protect the audio data in own memory area while playing it
	CopyMemory( &buffers[ currentBuffer * soundBufferLen ], finalWave, soundBufferLen );

	buf.AudioBytes = soundBufferLen;
	buf.pAudioData = &buffers[ currentBuffer * soundBufferLen ];

	currentBuffer++;
	currentBuffer %= ( bufferCount + 1 ); // + 1 because we need one temporary buffer

	HRESULT hr = sVoice->SubmitSourceBuffer( &buf ); // send buffer to queue
	ASSERT( hr == S_OK );
}


void XAudio2_Output::pause()
{
	if( !initialized || failed ) return;

	if( playing ) {
		HRESULT hr = sVoice->Stop( 0 );
		ASSERT( hr == S_OK );
		playing = false;
	}
}


void XAudio2_Output::resume()
{
	if( !initialized || failed ) return;

	if( !playing ) {
		HRESULT hr = sVoice->Start( 0 );
		ASSERT( hr == S_OK );
		playing = true;
	}
}


void XAudio2_Output::reset()
{
	if( !initialized || failed ) return;

	if( playing ) {
		HRESULT hr = sVoice->Stop( 0 );
		ASSERT( hr == S_OK );
	}

	sVoice->FlushSourceBuffers();
	sVoice->Start( 0 );
	playing = true;
}


void XAudio2_Output::setThrottle( unsigned short throttle )
{
	if( !initialized || failed ) return;

	if( throttle == 0 ) throttle = 100;
	HRESULT hr = sVoice->SetFrequencyRatio( (float)throttle / 100.0f );
	ASSERT( hr == S_OK );
}

void xaudio2_device_changed( XAudio2_Output * instance )
{
	instance->device_change();
}

SoundDriver *newXAudio2_Output()
{
	return new XAudio2_Output();
}


#endif // #ifndef NO_XAUDIO2
