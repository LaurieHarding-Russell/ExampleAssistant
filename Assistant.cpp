#include <thread>
#include <sstream>
#include <iostream>
#include <regex>

#include <pocketsphinx.h>
#include "sphinxbase/jsgf.h"
#include "sphinxbase/fsg_model.h"
#include "sphinxbase/ad.h"
#include "sphinxbase/err.h"

#include "pocketsphinx_internal.h"
#include "fsg_search_internal.h"

extern"C" {
	#include "flite.h"
	cst_voice* register_cmu_us_slt(const char *voxdir);
	void unregister_cmu_us_slt(cst_voice *vox);
}

cmd_ln_t* initializeGrammer();
void inputLoop(cmd_ln_t* config, ps_decoder_t* ps);
bool commandInterface(std::string& userCommand, ad_rec_t *ad);
void say(char* text, ad_rec_t *ad);
void say(char* text);

cst_voice* voice;

std::string currentCommand = "";

// FIXME, should probably wrap the pocketsphinx stuff in a class
cmd_ln_t* config = initializeGrammer();
ps_decoder_t* ps = ps_init(config);
ad_rec_t *ad;

int main() {
	flite_init();
	voice = register_cmu_us_slt(NULL);

	flite_text_to_speech("Hello, I'm awake!", voice, "play");

    cmd_ln_t* config = initializeGrammer();
    ps_decoder_t* ps = ps_init(config);

    inputLoop(config, ps);
    
	unregister_cmu_us_slt(voice);
	ps_free(ps);
	cmd_ln_free_r(config);

    return 0;
}


bool commandInterface(std::string& userCommand, ad_rec_t *ad) {
    std::regex insult ("(computer|machine|skynet)");
    std::regex math ("what is");
    std::regex exit ("(bye | goodbye)");


    currentCommand +=" " + userCommand;
    std::cerr << currentCommand << '\n';
    userCommand = "";
    if (regex_search(currentCommand, insult)) {
        say("i know you are but what am i?", ad);
        currentCommand = "";
    } 
    else if (regex_search(currentCommand, exit)) {
        say("bye", ad);
        currentCommand = "";
        return true;
    }
    else if (regex_search(currentCommand, math)) {
        say("I'm not a calculator", ad);
        currentCommand = "";
    }

    userCommand = "";
    return false;
}

void inputLoop(cmd_ln_t* config, ps_decoder_t* ps) {
    std::string userCommand = "";

    ad_rec_t *ad;
    int16 adbuf[8192];
    uint8 utt_started, in_speech;
    int32 input;
    char const *hyp;
    bool exit = false;

    if ((ad = ad_open_dev(cmd_ln_str_r(config, "-adcdev"),
                            (int) cmd_ln_float32_r(config,
                            "-samprate"))) == NULL) 
    {
            exit = true;
    }
    if (ad_start_rec(ad) < 0) {
            exit = true;
    }

    if (ps_start_utt(ps) < 0) {
            exit = true;
    }


	std::string command = "";
	while (!exit) {
        input = ad_read(ad, adbuf, 8191);


		ps_process_raw(ps, adbuf, input, false, false);
		in_speech = ps_get_in_speech(ps);
		if (in_speech && !utt_started) {
  			utt_started = TRUE;
		}
		if (!in_speech && utt_started) {
			// speech -> silence transition, time to start new utterance
 			ps_end_utt(ps);
			hyp = ps_get_hyp(ps, NULL );
			if (hyp != NULL) {
				std::istringstream iss(hyp);
				while (std::getline(iss, command, ' ')) {
					userCommand += command;
                    exit = commandInterface(userCommand, ad);
				}
			}

			if (ps_start_utt(ps) < 0){

			}
			utt_started = FALSE;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ad_close(ad);
}

cmd_ln_t* initializeGrammer() {
    err_set_logfile("pocketSphinx_Log.txt");
    return cmd_ln_init(NULL, ps_args(), TRUE,
                "-hmm",  "external/pocketsphinx/model/en-us/en-us",
                "-dict", "external/pocketsphinx/model/en-us/cmudict-en-us.dict",
                "-jsgf", "example.gram",
                NULL);
}


void say(char* text) {
    flite_text_to_speech(text, voice, "play");
}

void say(char* text, ad_rec_t *ad) {
    ad_stop_rec(ad);
    flite_text_to_speech(text, voice, "play");
    ad_start_rec(ad);
}