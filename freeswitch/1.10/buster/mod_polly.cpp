/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Daniel Donoghue <daniel.donoghue@freespee.com>
 *
 *
 * mod_polly.c -- Test Playground for labs
 *
 */

extern "C" {
#include <switch.h>
}


#include <fstream>

#define BIG_ENDIAN_SYSTEM (*(uint16_t *)"\0\xff" < 0x100)

#define REVERSE_BYTES(...) do for(size_t REVERSE_BYTES=0; REVERSE_BYTES<sizeof(__VA_ARGS__)>>1; ++REVERSE_BYTES)\
	((unsigned char*)&(__VA_ARGS__))[REVERSE_BYTES] ^= ((unsigned char*)&(__VA_ARGS__))[sizeof(__VA_ARGS__)-1-REVERSE_BYTES],\
	((unsigned char*)&(__VA_ARGS__))[sizeof(__VA_ARGS__)-1-REVERSE_BYTES] ^= ((unsigned char*)&(__VA_ARGS__))[REVERSE_BYTES],\
	((unsigned char*)&(__VA_ARGS__))[REVERSE_BYTES] ^= ((unsigned char*)&(__VA_ARGS__))[sizeof(__VA_ARGS__)-1-REVERSE_BYTES];\
while(0)

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/Outcome.h>
#include <aws/polly/PollyClient.h>
#include <aws/polly/model/SynthesizeSpeechRequest.h>
#include <aws/polly/model/SynthesizeSpeechResult.h>
#include <aws/polly/model/TextType.h>
#include <aws/polly/model/LanguageCode.h>
#include <aws/polly/model/OutputFormat.h>
#include <aws/polly/model/VoiceId.h>

typedef unsigned long DWORD;    // 32-bit unsigned integer
typedef unsigned short WORD;    // 16-bit unsigned integer

struct riff									// Data             Bytes	Total
{
	char			chunkID[4];		        // "RIFF"		    4		4
	DWORD			riffSize;		        // file size - 8	4		8
	char			typeID[4];		        // "WAVE"		    4		12
	char			formatChunkID[4];	    // "fmt "		    4		16
	DWORD			formatChunkSize;	    // 16 bytes		    4		20			
	WORD			formatTag;		        //		        	2		22
	WORD			noOfChannels;		    //		        	2		24
	DWORD			samplesPerSec;		    //		        	4		28
	DWORD			bytesPerSec;		    //	        		4		32
	WORD			blockAlign;		        //	        		2		34
	WORD			bitsPerSample;		    //		        	2		36
	char			dataChunkID[4];		    // "data"	    	4		40
	DWORD			dataChunkSize;		    // not fixed	   	4		44
};

static struct {
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *running_rwlock;
	switch_memory_pool_t *pool;
	int running;
} process;

static struct {
	Aws::Auth::AWSCredentials *credentials;
	Aws::Client::ClientConfiguration *config;
	Aws::SDKOptions *options;
} globals;

switch_loadable_module_interface_t *MODULE_INTERFACE;

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

/* Prototypes */

SWITCH_MODULE_LOAD_FUNCTION(mod_polly_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_polly_shutdown);
SWITCH_MODULE_DEFINITION(mod_polly, mod_polly_load, mod_polly_shutdown, NULL);

// ------------------------------------------------------------------------------------------------------------------

/* Implementation */

std::ostream& operator<<(std::ostream& out, const riff& h)
{
	if BIG_ENDIAN_SYSTEM {
		struct riff hdr = std::move(h);

		REVERSE_BYTES(hdr.riffSize);
		REVERSE_BYTES(hdr.formatChunkSize);
		REVERSE_BYTES(hdr.formatTag);
		REVERSE_BYTES(hdr.noOfChannels);
		REVERSE_BYTES(hdr.samplesPerSec);
		REVERSE_BYTES(hdr.bytesPerSec);
		REVERSE_BYTES(hdr.blockAlign);
		REVERSE_BYTES(hdr.bitsPerSample);
		REVERSE_BYTES(hdr.dataChunkSize);

		return out 
			.write(hdr.chunkID, 4)
			.write((const char *)&hdr.riffSize, 4)
			.write(hdr.typeID, 4)
			.write(hdr.formatChunkID, 4)
			.write((const char *)&hdr.formatChunkSize, 4)
			.write((const char *)&hdr.formatTag, 2)
			.write((const char *)&hdr.noOfChannels, 2)
			.write((const char *)&hdr.samplesPerSec, 4)
			.write((const char *)&hdr.bytesPerSec, 4)
			.write((const char *)&hdr.blockAlign, 2)
			.write((const char *)&hdr.bitsPerSample, 2)
			.write(hdr.dataChunkID, 4)
			.write((const char *)&hdr.dataChunkSize, 4);
	} else {
		return out
			.write(h.chunkID, 4)
			.write((const char *)&h.riffSize, 4)
			.write(h.typeID, 4)
			.write(h.formatChunkID, 4)
			.write((const char *)&h.formatChunkSize, 4)
			.write((const char *)&h.formatTag, 2)
			.write((const char *)&h.noOfChannels, 2)
			.write((const char *)&h.samplesPerSec, 4)
			.write((const char *)&h.bytesPerSec, 4)
			.write((const char *)&h.blockAlign, 2)
			.write((const char *)&h.bitsPerSample, 2)
			.write(h.dataChunkID, 4)
			.write((const char *)&h.dataChunkSize, 4);
	}
}

riff init_pcm_header(std::ostream& in)
{
	// get length of file
	in.seekp(0, in.end);
	DWORD sz = in.tellp();
	in.seekp(0, in.beg);

	struct riff result = {
		{'R','I','F','F'},      // chunkID
		sz + 0x24,              // riffSize         (size of stream + 0x24) or (file size - 8)
		{'W','A','V','E'},      // typeID
		{'f','m','t',' '},      // formatChunkID
		16,                     // formatChunkSize
		1,                      // formatTag        (PCM)
		1,                      // noOfChannels     (mono)
		8000,                   // samplesPerSec    (8KHz)
		16000,                  // bytesPerSec      ((Sample Rate * BitsPerSample * Channels) / 8)
		2,                      // blockAlign       ((bits per sample * channels) / 8)
		16,                     // bitsPerSample    (multiples of 8)
		{'d','a','t','a'},      // dataChunkID
		sz                      // dataChunkSize    (sample size)   
	};

	return result;
}

struct voice_sync {
	char* session_uuid;
	Aws::IOStream *audio_stream;
	switch_size_t blockAlign;
};

typedef struct voice_sync voice_sync_t;

static switch_status_t polly_file_open(switch_file_handle_t *handle, const char *path)
{
	voice_sync_t *sync_info = (voice_sync_t*)malloc(sizeof(voice_sync_t));
	sync_info->audio_stream = new Aws::StringStream(std::ios::in | std::ios::out | std::ios::binary);

	handle->private_info = sync_info;
	handle->samplerate = 8000;
	handle->channels = 1;
	handle->pos = 0;
	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0.5;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "submitting text [%s] to polly", path);

	Aws::Polly::PollyClient polly_client(*globals.credentials, *globals.config);
	Aws::Polly::Model::SynthesizeSpeechRequest request;

	request.SetLanguageCode(Aws::Polly::Model::LanguageCode::en_US);
	request.SetOutputFormat(Aws::Polly::Model::OutputFormat::pcm);
	request.SetSampleRate("8000");
	request.SetTextType(Aws::Polly::Model::TextType::text);  // or ssml
	request.SetVoiceId(Aws::Polly::Model::VoiceId::Matthew);
	request.SetText(path);

	if (handle->params) {
		// get the session UUID for this channel
		// note: this doesnt fire for a standard call session in the audio context; is there a way to make sure it is there?
		const char *uuid = switch_event_get_header(handle->params, "session");
		if (!zstr(uuid)) {
			sync_info->session_uuid = switch_core_strdup(handle->memory_pool, uuid);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(sync_info->session_uuid), SWITCH_LOG_DEBUG, "Polly linked to session %s\n", sync_info->session_uuid);
		}
	}
	sync_info->audio_stream->clear();
//	  sync_info->audio_stream.open(filename.c_str(), std::ios::out | std::ios::binary);

	auto outcome = polly_client.SynthesizeSpeech(request);

	// Output operation status
	if (outcome.IsSuccess()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "received audio response for %s", request.GetServiceRequestName());

		Aws::Polly::Model::SynthesizeSpeechResult& result = ((Aws::Polly::Model::SynthesizeSpeechResult&)(outcome));
		Aws::IOStream*  audio_stream = &result.GetAudioStream();

		// this is raw PCM so we need to add a wav header!
		riff header = init_pcm_header(*audio_stream);
		*sync_info->audio_stream << header;

		// tansfer audio data into stream
		*sync_info->audio_stream << audio_stream->rdbuf();
		sync_info->audio_stream->seekp(0, sync_info->audio_stream->beg);

		// update handle information about audio stream
		handle->samplerate = header.samplesPerSec;
		handle->channels = header.noOfChannels;
		handle->format = header.formatTag;
		handle->duration = header.dataChunkSize / header.bytesPerSec +1;
		handle->samples_in = header.dataChunkSize / header.blockAlign +1;

		sync_info->blockAlign = header.blockAlign;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "polly audio stream ready; duration: %ld secs", handle->duration);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "something went wrong retrieving audio from polly");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t polly_file_close(switch_file_handle_t *handle)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "closiing polly audio stream");

	voice_sync_t *sync_info = (voice_sync_t*)handle->private_info;

	//sync_info->audio_stream->close();	-- doesnt exist on stringstream
	delete sync_info->audio_stream;

	if (sync_info->session_uuid) {
		switch_safe_free(sync_info->session_uuid);
	}

	switch_safe_free(sync_info);
	handle->private_info = NULL;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t polly_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	voice_sync_t *sync_info = (voice_sync_t*)handle->private_info;
	switch_size_t bytes;

	sync_info->audio_stream->read((char *)data, *len * sync_info->blockAlign);
	if ((bytes = sync_info->audio_stream->gcount()) <= 0) {
		return SWITCH_STATUS_FALSE;
	}

	*len = bytes / sync_info->blockAlign;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_polly_load)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Initializing polly audio interface");

	supported_formats[0] = (char*)"polly";

	/*
		switch_application_interface_t *app_interface;
		switch_api_interface_t *api_interface;
	*/
	switch_file_interface_t *file_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = (switch_file_interface_t*)switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = polly_file_open;
	file_interface->file_close = polly_file_close;
	file_interface->file_read = polly_file_read;

	MODULE_INTERFACE = *module_interface;

	memset(&process, 0, sizeof(process));
	memset(&globals, 0, sizeof(globals));
	process.pool = pool;

	switch_thread_rwlock_create(&process.running_rwlock, pool);
	switch_mutex_init(&process.mutex, SWITCH_MUTEX_NESTED, pool);

	globals.options = new Aws::SDKOptions();
	globals.options->loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;

	globals.credentials = new Aws::Auth::AWSCredentials();
	globals.credentials->SetAWSAccessKeyId("your aws key");
	globals.credentials->SetAWSSecretKey("your aws secret");

	globals.config = new Aws::Client::ClientConfiguration();
	globals.config->region = "eu-west-1";  // Ireland

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Initializing aws api");

	Aws::InitAPI(*globals.options);

	switch_thread_rwlock_wrlock(process.running_rwlock);
	process.running = 1;
	switch_thread_rwlock_unlock(process.running_rwlock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ready to rock!");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_polly_shutdown)
{   
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutting down polly polly audio interface");

	switch_thread_rwlock_wrlock(process.running_rwlock);
	process.running = 0;
	switch_thread_rwlock_unlock(process.running_rwlock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Closing aws api");
	Aws::ShutdownAPI(*globals.options);

	delete globals.credentials;
	delete globals.config;
	delete globals.options;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module shutdown finished");
	return SWITCH_STATUS_UNLOAD;
}
