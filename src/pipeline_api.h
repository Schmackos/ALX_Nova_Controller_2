#ifndef PIPELINE_API_H
#define PIPELINE_API_H

#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

class WebServer;
void registerPipelineApiEndpoints(WebServer& server);
void pipeline_api_check_deferred_save();

#endif // NATIVE_TEST
#endif // DAC_ENABLED

#endif // PIPELINE_API_H
