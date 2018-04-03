#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <json-c/json.h>
#include <json-c/json_util.h>
#include <iostream>

typedef struct
{
    unsigned int  flag;
    char channel_id[32];
    char project_id[128];
    char platform_id[128];
    char movie_id[128];
    long timestamp;
}image_stream_t;

static size_t max_json_len = 1000000;//1Mb

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    if (strlen((char *)stream) + strlen((char *)ptr) > max_json_len-1)
        return 0;
    strcat((char *)stream, (char *)ptr);
    return size * nmemb;
}
int upload_image(const char *json_data,const char *curl_url){

    CURL *curl;
    CURLcode res;
    char dir_name[256] = {0};
    char file_name[128] = {0};
    char image_file_path[256] = {0};
    char temp[256] = {0};
    struct tm *local_tmp;


    json_object *jobj = NULL;
    json_object *jtemp = NULL;
    json_object *jchannels = NULL;
    int json_len= 0;
    jobj = json_tokener_parse(json_data);
    image_stream_t image_stream ={0};
    json_pointer_get(jobj,"/project_id",&jtemp);
    strcpy(image_stream.project_id, json_object_get_string(jtemp));
    json_pointer_get(jobj,"/channel_id",&jtemp);
    strcpy(image_stream.channel_id, json_object_get_string(jtemp));
    json_pointer_get(jobj,"/nvr_sn",&jtemp);
    strcpy(image_stream.platform_id, json_object_get_string(jtemp));
    json_pointer_get(jobj,"/movie_id",&jtemp);
    strcpy(image_stream.movie_id, json_object_get_string(jtemp));
    long timestamp;
    
    json_pointer_get(jobj, "/time", &jchannels);
    printf("json_object_get_type:%d\n", json_object_get_type(jchannels));
    if(json_object_get_type(jchannels) != json_type_array){
        return -1;
    }
    json_len = json_object_array_length(jchannels);
    // CHANNEL_INFO *data = NULL;// = (CHANNEL_INFO **)malloc(sizeof(CHANNEL_INFO *) * json_len);
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    static const char buf[] = "Expect:";

    char *buffer = (char *)malloc(max_json_len);
    for (int i = 0; i < json_len; i++)
    {
        memset(buffer, 0, max_json_len);
        json_object *val = json_object_array_get_idx(jchannels, i);
        timestamp = json_object_get_int64(val);
        local_tmp = gmtime(&(timestamp));
        sprintf(dir_name, "image/%s-%s-%s/%04d-%02d/%04d-%02d-%02d", image_stream.project_id, image_stream.platform_id, image_stream.channel_id, (1900 + local_tmp->tm_year), (1 + local_tmp->tm_mon),
                (1900 + local_tmp->tm_year), (1 + local_tmp->tm_mon), local_tmp->tm_mday);

        sprintf(file_name, "%s-%s-%ld.jpg", image_stream.platform_id, image_stream.channel_id, timestamp);
        sprintf(image_file_path, "%s/images/%s", dir_name, file_name);
        printf("detector_xml:%s\n", image_file_path);


        curl_global_init(CURL_GLOBAL_ALL);

        /* Fill in the file upload field */
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "sendfile",
                     CURLFORM_FILE, image_file_path,
                     CURLFORM_END);

        /* Fill in the filename field */
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "filename",
                     CURLFORM_COPYCONTENTS, file_name,
                     CURLFORM_END);

        /* Fill in the submit field too, even if this is rarely needed */
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "submit",
                     CURLFORM_COPYCONTENTS, "Submit",
                     CURLFORM_END);

        curl = curl_easy_init();
        /* initalize custom header list (stating that Expect: 100-continue is not 
    wanted */
        headerlist = curl_slist_append(headerlist, buf);
        if (curl)
        {
            /* what URL that receives this POST  <span style = "white-space:pre"></ span> */
            char buf[512] = {0};
            sprintf(buf, "%s?movie_id=%s", "http://ai.gandianli.com/Index/Index/_getImgUploadOss", image_stream.movie_id);////
            printf("buf:%s\n", buf);
            curl_easy_setopt(curl, CURLOPT_URL, buf);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3); //设置超时时间
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
            if (res != CURLE_OK)
            {
                fprintf(stderr, "Failed to set write data \n"); //[%s]
                return -1;
            }

            // if ((argc == 2) && (!strcmp(argv[1], "noexpectheader")))
            //     /* only disable 100-continue header if explicitly requested */
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
            curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);
            /* Check for errors */
            if (res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                        curl_easy_strerror(res));
            printf("buffer:%s\n", buffer);
        }
    }
    /* always cleanup */
    curl_easy_cleanup(curl);
    free(buffer);
    /* then cleanup the formpost chain */
    curl_formfree(formpost);
    /* free slist */
    curl_slist_free_all(headerlist);
}
int main(int argc, char *argv[])
{
    const char* curl_url = "https://";
    AmqpClient::Channel::ptr_t channel = AmqpClient::Channel::Create("114.55.207.43");
    std::string exchange_name = "violation-deal";
    channel->DeclareExchange(exchange_name,
                             AmqpClient::Channel::EXCHANGE_TYPE_DIRECT, false, true, false);//EXCHANGE_TYPE_FANOUT
    std::string queue_name = channel->DeclareQueue("violation-deal.ttx.msg.q", false, true, false, false);
    channel->BindQueue(queue_name, exchange_name, "ttx.msg.q");
    std::string consumer_tag =
        channel->BasicConsume(queue_name, "", true, true, false, 1);

    while (1){
        std::cout << "等待接收信息中" << std::endl;
        AmqpClient::Envelope::ptr_t envelope =
            channel->BasicConsumeMessage(consumer_tag);
        std::string buffer = envelope->Message()->Body();
        std::cout << "[y] receve " << buffer << std::endl;
        upload_image(buffer.c_str(), curl_url);
    }
    
    return 0;
}




































        // video_stream_t *video_stream = (video_stream_t*)malloc(sizeof(video_stream_t));
        // json_object *val = json_object_array_get_idx(jchannels, i);
        // // data = (CHANNEL_INFO*) malloc(sizeof(CHANNEL_INFO));
        // json_pointer_get(val,"/project_id",&jtemp);
        // strcpy(video_stream->project_id, json_object_get_string(jtemp));
        // // printf("video_stream->project_id:%s\n",video_stream->project_id);
        // json_pointer_get(val,"/channel",&jtemp);
        // strcpy(video_stream->channel_id, json_object_get_string(jtemp));
        // json_pointer_get(val,"/nvr_sn",&jtemp);
        // strcpy(video_stream->platform_id, json_object_get_string(jtemp));
        // json_pointer_get(val,"/url",&jtemp);
        // strcpy(video_stream->video_url, json_object_get_string(jtemp));
        // // data->is_stop = 0;
        // data_len_c ++;

        // if(pthread_create(&(video_stream->video2image_pthread),NULL,image_thread,(void *)(video_stream))!=0)return -1;
        // list_add_tail(&(video_stream->video_stream_node), head);
        // printf("%s-%s add success!\n",video_stream->channel_id,video_stream->platform_id);



// int deal_json(const char *json,const char *curl_url){

//     CURL *curl;
//     CURLcode res;

//     struct curl_httppost *formpost = NULL;
//     struct curl_httppost *lastptr = NULL;
//     struct curl_slist *headerlist = NULL;
//     static const char buf[] = "Expect:";

//     curl_global_init(CURL_GLOBAL_ALL);

//     /* Fill in the file upload field */
//     curl_formadd(&formpost,
//                  &lastptr,
//                  CURLFORM_COPYNAME, "sendfile",
//                  CURLFORM_FILE, "D:\\sign.txt",
//                  CURLFORM_END);

//     /* Fill in the filename field */
//     curl_formadd(&formpost,
//                  &lastptr,
//                  CURLFORM_COPYNAME, "filename",
//                  CURLFORM_COPYCONTENTS, "sign.txt",
//                  CURLFORM_END);

//     /* Fill in the submit field too, even if this is rarely needed */
//     curl_formadd(&formpost,
//                  &lastptr,
//                  CURLFORM_COPYNAME, "submit",
//                  CURLFORM_COPYCONTENTS, "Submit",
//                  CURLFORM_END);

//     curl = curl_easy_init();
//     /* initalize custom header list (stating that Expect: 100-continue is not 
//  wanted */
//     // headerlist = curl_slist_append(headerlist, buf);
//     if (curl)
//     {
//         /* what URL that receives this POST  <span style = "white-space:pre"></ span> */
//        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/fileUpload.action");
//         if ((argc == 2) && (!strcmp(argv[1], "noexpectheader")))
//             /* only disable 100-continue header if explicitly requested */
//             curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
//         curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

//         /* Perform the request, res will get the return code */
//         res = curl_easy_perform(curl);
//         /* Check for errors */
//         if (res != CURLE_OK)
//             fprintf(stderr, "curl_easy_perform() failed: %s\n",
//                     curl_easy_strerror(res));

//         /* always cleanup */
//         curl_easy_cleanup(curl);

//         /* then cleanup the formpost chain */
//         curl_formfree(formpost);
//         /* free slist */
//         curl_slist_free_all(headerlist);
//     }
// }
