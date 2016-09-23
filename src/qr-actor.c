/*
 * qr-actor.c
 *
 *  Created on: Aug 29, 2016
 *      Author: ChauNM
 */
#include "qr-actor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "lib/jansson/jansson.h"
#include "universal/typesdef.h"
#include "universal/universal.h"

static PACTOR qrActor = NULL;

static void QrActorCreate(char* guid, char* psw, char* host, WORD port)
{
	qrActor = ActorCreate(guid, psw, host, port);
	//Register callback to handle request package
	if (qrActor == NULL)
	{
		printf("Couldn't create actor\n");
		return;
	}
	//ActorRegisterCallback(pLedActor, ":request/turn_on", LedActorOnRequestTurnOn, CALLBACK_RETAIN);
	//ActorRegisterCallback(pLedActor, ":request/turn_off", LedActorOnRequestTurnOff, CALLBACK_RETAIN);
	//ActorRegisterCallback(pLedActor, ":request/blink", LedActorOnRequestBlink, CALLBACK_RETAIN);
}

static void QrActorPublishQrContent(char* content)
{
	if (qrActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* statusJson = json_string("status.success");
	json_t* contentJson = json_string(content);
	json_object_set(paramsJson, "status", statusJson);
	json_object_set(paramsJson, "content", contentJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(qrActor->guid, "/:event/qr_update");
	ActorSend(qrActor, topicName, eventMessage, NULL, FALSE);
	json_decref(statusJson);
	json_decref(contentJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

static void QrActorPublishInfo(char* info)
{
	if (qrActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* infoJson = json_string(info);
	json_object_set(paramsJson, "info", infoJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(qrActor->guid, "/:event/info");
	ActorSend(qrActor, topicName, eventMessage, NULL, FALSE);
	json_decref(infoJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

static FILE* QrProcessStart(char* device, int width, int height)
{
	char command[256];
	sprintf(command, "LD_PRELOAD=/usr/lib/arm-linux-gnueabihf/libv4l/v4l1compat.so zbarcam %s --prescale=%dx%d --nodisplay", device, width, height);
	printf("command: %s\n", command);
	FILE *fp = popen(command, "r");
	return fp;
}

static void QrProcess(FILE* fp)
{
	char stdoutContent[1024];
	char qrContent[1024];
	memset(stdoutContent, 0, sizeof(stdoutContent));
	if (fgets(stdoutContent, sizeof(stdoutContent), fp) != NULL)
	{
		if (strstr(stdoutContent, "QR-Code:") != NULL)
		{
			memset(qrContent, 0, sizeof(qrContent));
			strcpy(qrContent, stdoutContent + 8);
			printf("QR-content: %s\n", qrContent);
			QrActorPublishQrContent(qrContent);
		}
		else
			QrActorPublishInfo(stdoutContent);
	}
}

void QrActorStart(PQRACTOROPTION pQrOption)
{
	FILE* fp;
	PACTOROPTION option = &(pQrOption->actorOption);
	mosquitto_lib_init();
	printf("Create actor\n");
	QrActorCreate(option->guid, option->psw, option->host, option->port);
	if (qrActor == NULL)
	{
		mosquitto_lib_cleanup();
		return;
	}
	printf("Start QR process\n");
	fp = QrProcessStart(pQrOption->camDevice, pQrOption->imageWidth, pQrOption->imageHeight);
	while(1)
	{
		ActorProcessEvent(qrActor);
		QrProcess(fp);
		mosquitto_loop(qrActor->client, 0, 1);
		usleep(10000);
	}
	mosquitto_disconnect(qrActor->client);
	mosquitto_destroy(qrActor->client);
	mosquitto_lib_cleanup();
}

int main(int argc, char* argv[])
{
	/* get option */
	int opt= 0;
	char *token = NULL;
	char *guid = NULL;
	char *host = NULL;
	char *camDevice = NULL;
	int imageWidth;
	int imageHeight;
	WORD port = 0;
	camDevice = StrDup("/dev/video0");
	imageHeight = 480;
	imageWidth = 640;
	printf("start qr-service \n");
	// specific the expected option
	static struct option long_options[] = {
			{"id",      required_argument,  0, 'i' },
			{"token", 	required_argument,  0, 't' },
			{"host", 	required_argument,  0, 'H' },
			{"port", 	required_argument, 	0, 'p' },
			{"cam", 	required_argument,	0, 'c' },
			{"width",	required_argument, 	0, 'd' },
			{"height", 	required_argument, 	0, 'r' },
			{"help", 	no_argument, 		0, 'h'	}
	};
	int long_index;
	/* Process option */
	while ((opt = getopt_long(argc, argv,":hi:t:H:p:c:d:r:",
			long_options, &long_index )) != -1) {
		switch (opt) {
		case 'h' :
			printf("using: qr-service --<token> --<id> --<host> --port<>\n"
					"id: guid of the actor\n"
					"token: password of the actor\n"
					"host: mqtt server address, if omitted using local host\n"
					"port: mqtt port, if omitted using port 1883\n"
					"cam: camera device, default is /dev/video0\n"
					"width: image width, default is 640, should larger than 320\n"
					"height: image height, default is 480, should larger than 320\n");
			return (EXIT_SUCCESS);
			break;
		case 'i':
			guid = StrDup(optarg);
			break;
		case 't':
			token = StrDup(optarg);
			break;
		case 'H':
			host = StrDup(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			imageWidth = atoi(optarg);
			if (imageWidth < 320)
			{
				printf("invalid options, image width is too small");
				return EXIT_FAILURE;
			}
			break;
		case 'r':
			imageHeight = atoi(optarg);
			if (imageHeight < 320)
			{
				printf("invalid options, image height is too small");
				return EXIT_FAILURE;
			}
			break;
		case 'c':
			camDevice = StrDup(optarg);
			break;
		case ':':
			if (optopt == 'i')
			{
				printf("invalid option(s), using --help for help\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			break;
		}
	}
	if (guid == NULL)
	{
		printf("invalid option(s), using --help for help\n");
		return EXIT_FAILURE;
	}
	QRACTOROPTION qrActorOpt;
	qrActorOpt.actorOption.guid = guid;
	qrActorOpt.actorOption.psw = token;
	qrActorOpt.actorOption.port = port;
	qrActorOpt.actorOption.host = host;
	qrActorOpt.camDevice = camDevice;
	qrActorOpt.imageWidth = imageWidth;
	qrActorOpt.imageHeight = imageHeight;
	QrActorStart(&qrActorOpt);
	return EXIT_SUCCESS;
}

