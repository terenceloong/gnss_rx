// Not using precompiled headers
// Character set: No set
// Multithread version

#define _CRT_SECURE_NO_WARNINGS
#define HAVE_STRUCT_TIMESPEC

// UHD manual: http://files.ettus.com/manual/usrp_8h.html

#include <uhd.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include "getopt.h"
#include "nmea/nmea.h"

/*------------------------------------------------------------------------*/

void* write_data(void* arg);

sem_t sem;

#define BUFF_LENGTH    50
int16_t *buff_ch1[BUFF_LENGTH];
int16_t *buff_ch2[BUFF_LENGTH];
uint16_t buff_head = 0;
uint16_t buff_tail = 0;

FILE *fp_ch1 = NULL;
FILE *fp_ch2 = NULL;

char file_path[150] = "";

/*------------------------------------------------------------------------*/

#define DISPLAY_UHD_ERROR(message) \
        uhd_get_last_error(uhd_error_str, sizeof(uhd_error_str)); \
        printf(""#message" failed!\n"); \
        printf("UHD error code: %d\n", uhd_error_code); \
        printf("%s\n", uhd_error_str); \
        return 1;

int main(int argc, char* argv[])
{
    // Default RF parameter
    double freq = 1575.42e6;
    double master_rate = 16e6;
    double rate = 4e6;
    double gain = 32;
    size_t samps_per_buff = 400000;
    size_t channel[2] = {0,1};
    int n_channels = 1;

    // Default control parameter
    int t_samples = 5 * 10; //total sample time, unit:0.1s
    int t_discard = 2 * 10; //samples are discarded, unit:0.1s
    BOOL gps_flag = false; //whether synchronize with GPS time
    BOOL ref_flag = false; //whether use external clock
    BOOL name_flag = false; //whether use time name file
    char com[6] = "COM2";
    DWORD baudrate = 115200;

    int option = 0;
    //If append a parameter, a colon behind the letter
    while((option = getopt(argc, argv, "a:b:c:de:f:g:h:i:j:k:l:m:no:p:q:rs:t:u:v:w:x:y:z:")) != -1)
    {
        switch(option)
        {
            case 't': //total sample time
                t_samples = atoi(optarg) * 10;
                break;
            case 'p': //file path
                strcpy(file_path, optarg);
                break;
            case 's': //synchronize with GPS time
                gps_flag = true;
                strcpy(com, optarg);
                break;
            case 'b': //baudrate
                baudrate = atoi(optarg);
                break;
            case 'd': //double channels
                n_channels = 2;
                break;
            case 'g': //gain
                gain = atoi(optarg);
                break;
            case 'r': //use external clock
                ref_flag = true;
                break;
            case 'n': //use time name file
                name_flag = true;
                break;
            case 'f': //frequency
                freq = atof(optarg);
                break;
            default:
                break;
        }
    }

    //----1.Set thread priority
    uhd_error uhd_error_code;
    char uhd_error_str[500];
    //uhd_error_code = uhd_set_thread_priority(uhd_default_thread_priority, true);
    uhd_error_code = uhd_set_thread_priority(1, true); //highest priority
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set thread priority)
    }
    
    //----2.Create USRP handle
    uhd_usrp_handle usrp;
    uhd_error_code = uhd_usrp_make(&usrp, "type=b200");
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Create USRP handle)
    }

    //----3.Set clock source
    char clock_source[20];
    if(ref_flag)
        uhd_error_code = uhd_usrp_set_clock_source(usrp, "external", 0);
    else
        uhd_error_code = uhd_usrp_set_clock_source(usrp, "internal", 0);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set clock source)
    }
    uhd_error_code = uhd_usrp_get_clock_source(usrp, 0, clock_source, sizeof(clock_source));
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Get clock source)
    }
    printf("Clock source: %s.\n", clock_source);

    //----4.Set time source
    char time_source[20];
    if(gps_flag)
        uhd_error_code = uhd_usrp_set_time_source(usrp, "external", 0);
    else
        uhd_error_code = uhd_usrp_set_time_source(usrp, "internal", 0);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set time source)
    }
    uhd_error_code = uhd_usrp_get_time_source(usrp, 0, time_source, sizeof(time_source));
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Get time source)
    }
    printf("Time source: %s.\n", time_source);

    //----5.Set master clock rate
    printf("Setting Master Clock Rate: %f MHz...\n", master_rate/1e6);
    uhd_error_code = uhd_usrp_set_master_clock_rate(usrp, master_rate, 0);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set master clock rate)
    }
    Sleep(100);

    //----6.Set sample rate
    double rate_a = 0; //actual rate
    /* channel 1 */
    {
        printf("Setting ch1 RX Rate: %f MHz...\n", rate/1e6);
        uhd_error_code = uhd_usrp_set_rx_rate(usrp, rate, channel[0]);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Set ch1 sample rate)
        }
        uhd_error_code = uhd_usrp_get_rx_rate(usrp, channel[0], &rate_a);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Get ch1 sample rate)
        }
        printf(" Actual ch1 RX Rate: %f MHz.\n", rate_a/1e6);
    }
    /* channel 2 */
    if(n_channels == 2)
    {
        printf("Setting ch2 RX Rate: %f MHz...\n", rate/1e6);
        uhd_error_code = uhd_usrp_set_rx_rate(usrp, rate, channel[1]);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Set ch2 sample rate)
        }
        uhd_error_code = uhd_usrp_get_rx_rate(usrp, channel[1], &rate_a);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Get ch2 sample rate)
        }
        printf(" Actual ch2 RX Rate: %f MHz.\n", rate_a/1e6);
    }

    //----7.Set reveive gain
    double gain_a = 0; //actual gain
    /* channel 1 */
    {
        printf("Setting ch1 RX Gain: %f dB...\n", gain);
        uhd_error_code = uhd_usrp_set_rx_gain(usrp, gain, channel[0], "");
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Set ch1 reveive gain)
        }
        uhd_error_code = uhd_usrp_get_rx_gain(usrp, channel[0], "", &gain_a);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Get ch1 reveive gain)
        }
        printf(" Actual ch1 RX Gain: %f dB.\n", gain_a);
    }
    /* channel 2 */
    if(n_channels == 2)
    {
        printf("Setting ch2 RX Gain: %f dB...\n", gain);
        uhd_error_code = uhd_usrp_set_rx_gain(usrp, gain, channel[1], "");
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Set ch2 reveive gain)
        }
        uhd_error_code = uhd_usrp_get_rx_gain(usrp, channel[1], "", &gain_a);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Get ch2 reveive gain)
        }
        printf(" Actual ch2 RX Gain: %f dB.\n", gain_a);
    }

    //----8.Set reveive frequency
    uhd_tune_request_t tune_request = 
    {
        .target_freq = freq,
        .rf_freq_policy  = UHD_TUNE_REQUEST_POLICY_AUTO,
        .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO
    };
    //uhd_tune_request_t tune_request = 
    //{
    //    .target_freq = freq,
    //    .rf_freq_policy  = UHD_TUNE_REQUEST_POLICY_MANUAL,
    //    .rf_freq = 1575.42e6,
    //    .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_MANUAL,
    //    .dsp_freq = 0
    //};
    uhd_tune_result_t tune_result;
    double freq_a = 0; //actual frequency
    /* channel 1 */
    {
        printf("Setting ch1 RX frequency: %f MHz...\n", freq/1e6);
        uhd_error_code = uhd_usrp_set_rx_freq(usrp, &tune_request, channel[0], &tune_result);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Set ch1 reveive frequency)
        }
        uhd_error_code = uhd_usrp_get_rx_freq(usrp, channel[0], &freq_a);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Get ch1 reveive frequency)
        }
        printf(" Actual ch1 RX frequency: %f MHz.\n", freq_a/1e6);
        printf("    clipped_rf_freq: %f\n", tune_result.clipped_rf_freq);
        printf("    target_rf_freq : %f\n", tune_result.target_rf_freq );
        printf("    actual_rf_freq : %f\n", tune_result.actual_rf_freq );
        printf("    target_dsp_freq: %f\n", tune_result.target_dsp_freq);
        printf("    actual_dsp_freq: %f\n", tune_result.actual_dsp_freq);
    }
    /* channel 2 */
    if(n_channels == 2)
    {
        printf("Setting ch2 RX frequency: %f MHz...\n", freq/1e6);
        uhd_error_code = uhd_usrp_set_rx_freq(usrp, &tune_request, channel[1], &tune_result);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Set ch2 reveive frequency)
        }
        uhd_error_code = uhd_usrp_get_rx_freq(usrp, channel[1], &freq_a);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Get ch2 reveive frequency)
        }
        printf(" Actual ch2 RX frequency: %f MHz.\n", freq_a/1e6);
        printf("    clipped_rf_freq: %f\n", tune_result.clipped_rf_freq);
        printf("    target_rf_freq : %f\n", tune_result.target_rf_freq );
        printf("    actual_rf_freq : %f\n", tune_result.actual_rf_freq );
        printf("    target_dsp_freq: %f\n", tune_result.target_dsp_freq);
        printf("    actual_dsp_freq: %f\n", tune_result.actual_dsp_freq);
    }

    //----9.Create RX streamer handle
    uhd_rx_streamer_handle rx_streamer;
    uhd_rx_streamer_make(&rx_streamer);
    uhd_stream_args_t stream_args = 
    {
        .cpu_format = "sc16",
        .otw_format = "sc16",
        .args = "",
        .channel_list = channel,
        .n_channels = n_channels
    };
    uhd_error_code = uhd_usrp_get_rx_stream(usrp, &stream_args, rx_streamer);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Get RX stream)
    }

    //----10.Create RX metadata handle
    uhd_rx_metadata_handle md;
    uhd_rx_metadata_make(&md);

    //----11.Check the max number of samples per buffer
    size_t max_num_samps;
    uhd_rx_streamer_max_num_samps(rx_streamer, &max_num_samps);
    printf("Max number of samples per buffer: %d.\n", max_num_samps);
    printf("Number of samples per buffer: %d.\n", samps_per_buff);
    //if(samps_per_buff > max_num_samps)
    //{
    //    printf("Number of samples per buffer is larger than maximum!\n");
    //    return 1;
    //}

    //----12.Get current time
    //Reference: https://blog.csdn.net/u012229282/article/details/79598287
    time_t timep;
    struct tm *tp;
    char date_str[20];
    char time_str[20];
    int hour_s, min_s, sec_s; //sample time
    time(&timep); //get current time in second
    tp = gmtime(&timep); //transform to time struct
    sprintf(date_str, "%4d%02d%02d_", tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday);
    printf("Host time: %02d-%02d-%02d\n", tp->tm_hour+8, tp->tm_min, tp->tm_sec);
    hour_s = tp->tm_hour + 8;
    min_s = tp->tm_min;
    sec_s = tp->tm_sec + 5; //sample after 5s
    if(sec_s >= 60)
    {
        sec_s = sec_s - 60;
        min_s++;
    }
    if(min_s == 60)
    {
        min_s = 0;
        hour_s++;
    }
    sprintf(time_str, "%02d%02d%02d_", hour_s, min_s, sec_s);

    //----13.Get GPS time
    //Reference: https://blog.csdn.net/lumanman_/article/details/76275513
    if(gps_flag)
    {
        HANDLE hCom;
        BOOL com_flag = false;
        for(int i=0; i<5; i++) //try 5 time
        {
            hCom = CreateFile(com, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if(hCom == INVALID_HANDLE_VALUE)
                printf("Open %s failed!\n", com);
            else
            {
                printf("Open %s succeeded!\n", com);
                com_flag = true;
                break;
            }
        }
        if(com_flag)
        {
            SetupComm(hCom, 1024, 1024); //queue length

            COMMTIMEOUTS TimeOuts; //ms
            TimeOuts.ReadIntervalTimeout = 50;
            TimeOuts.ReadTotalTimeoutMultiplier = 1;
            TimeOuts.ReadTotalTimeoutConstant = 1000;
            TimeOuts.WriteTotalTimeoutMultiplier = 1;
            TimeOuts.WriteTotalTimeoutConstant = 1;
            SetCommTimeouts(hCom, &TimeOuts);

            DCB dcb;
            GetCommState(hCom, &dcb);
            dcb.BaudRate = baudrate;
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            SetCommState(hCom, &dcb);

            DWORD wCount;
            BOOL bReadStat;
            char comBuff[1000];

            nmeaINFO info;
            nmeaPARSER parser;

            nmea_zero_INFO(&info);
            nmea_parser_init(&parser);

            while(1)
            {
                bReadStat = ReadFile(hCom, comBuff, sizeof(comBuff)-1, &wCount, NULL);
                if(wCount)
                {
                    comBuff[wCount] = 0;
                    printf("%s", comBuff);
                    printf("Count: %d\n", wCount);
                    if(comBuff[0]=='$' && comBuff[3]=='R' && comBuff[4]=='M' && comBuff[5]=='C')
                    {
                        nmea_parse(&parser, comBuff, wCount, &info);
                        if((info.fix==2) || (info.fix==3))
                        {
                            printf("lat: %f, lon: %f\n", info.lat, info.lon);
                            printf("GPS time: %02d-%02d-%02d\n", info.utc.hour+8, info.utc.min, info.utc.sec);
                            hour_s = info.utc.hour + 8;
                            min_s = info.utc.min;
                            sec_s = info.utc.sec + 5;
                            if(sec_s >= 60)
                            {
                                sec_s = sec_s - 60;
                                min_s++;
                            }
                            if(min_s == 60)
                            {
                                min_s = 0;
                                hour_s++;
                            }
                            sprintf(time_str, "%02d%02d%02d_", hour_s, min_s, sec_s);
                            break;
                        }
                    }
                }
            }

            nmea_parser_destroy(&parser);
            CloseHandle(hCom);
        }
    }

    //----14.Reset time
    time_t full_secs;
    double frac_secs;
    uhd_error_code = uhd_usrp_set_time_next_pps(usrp, 0, 0, 0);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Reset time)
    }
    printf("Reset time...\n");
    Sleep(1000);
    uhd_error_code = uhd_usrp_get_time_last_pps(usrp, 0, &full_secs, &frac_secs);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Get time last pps)
    }
    printf("Time last pps: %d, %.12f\n", (int)full_secs, frac_secs);

    //----15.Issue stream command
    // num_samps <= 0x0fffffff
    uhd_stream_cmd_t stream_cmd = 
    {
        //.stream_mode = UHD_STREAM_MODE_NUM_SAMPS_AND_DONE,
        //.num_samps = 4e6,
        .stream_mode = UHD_STREAM_MODE_START_CONTINUOUS,
        .stream_now = false,
        .time_spec_full_secs = (time_t)2.0,
        .time_spec_frac_secs = 0.0
    };
    uhd_error_code = uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Issue stream command)
    }

    //----16.Set GPIO
    // reference uhd example gpio.cpp, http://files.ettus.com/manual/classuhd_1_1usrp_1_1multi__usrp.html#a57f25d118d20311aca261e6dd252625e
    uhd_error_code = uhd_usrp_set_gpio_attr(usrp, "FP0", "CTRL", 1, 0x01, 0); //ATR mode, GPIO0
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set GPIO CTRL)
    }
    uhd_error_code = uhd_usrp_set_gpio_attr(usrp, "FP0", "DDR", 1, 0x01, 0); //output, GPIO0
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set GPIO DDR)
    }
    uhd_error_code = uhd_usrp_set_gpio_attr(usrp, "FP0", "ATR_RX", 1, 0x01, 0); //set at receiving, GPIO0
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Set GPIO ATR_RX)
    }

    //----17.Create output file
    // Create file failed, promote UAC permission, Properties->Linker->Manifest File->UAC Execution Level
    // reference: https://www.cnblogs.com/wainiwann/p/10065129.html
    if(file_path[0])
    {
        char file_name[200];
        /* channel 1 */
        {
            strcpy(file_name, file_path);
            strcat(file_name, "\\data_");
            if(name_flag)
            {
                strcat(file_name, date_str);
                strcat(file_name, time_str);
            }
            strcat(file_name, "ch1.dat");
            fp_ch1 = fopen(file_name, "wb");
            if(fp_ch1 == NULL)
            {
                printf("Create file failed! %s\n", file_name);
                return 1;
            }
            printf("Create file: %s\n", file_name);
        }
        /* channel 2 */
        if(n_channels == 2)
        {
            strcpy(file_name, file_path);
            strcat(file_name, "\\data_");
            if(name_flag)
            {
                strcat(file_name, date_str);
                strcat(file_name, time_str);
            }
            strcat(file_name, "ch2.dat");
            fp_ch2 = fopen(file_name, "wb");
            if(fp_ch2 == NULL)
            {
                printf("Create file failed! %s\n", file_name);
                return 1;
            }
            printf("Create file: %s\n", file_name);
        }
    }

    //----18.Create write data thread
    // reference: https://www.cnblogs.com/zhengAloha/p/8665719.html
    // reference: https://blog.csdn.net/baidu_35692628/article/details/69487525
    sem_init(&sem, 0, 0);
    pthread_t pt;
    int thread_para[2];
    thread_para[0] = n_channels;
    thread_para[1] = samps_per_buff;
    if(pthread_create(&pt, NULL, write_data, (void*)thread_para))
    {
        printf("Create thread failed!\n");
        return 1;
    }

    //----19.Reveive data
    int t_acc = 0; //unit:0.1s
    size_t num_rx_samps = 0;
    int16_t *buffs_ptr[2];
    uhd_rx_metadata_error_code_t md_error_code;
    char md_error_str[500];
    
    for(int i=0; i<BUFF_LENGTH; i++)
    {
        buff_ch1[i] = malloc(2 * sizeof(int16_t) * samps_per_buff);
        buff_ch2[i] = malloc(2 * sizeof(int16_t) * samps_per_buff);
        if(buff_ch1[i]==NULL || buff_ch2[i]==NULL)
        {
            printf("Apply for buffer space failed!\n");
            return 1;
        }
    }
    buffs_ptr[0] = buff_ch1[0];
    buffs_ptr[1] = buff_ch2[0];

    while(t_acc < t_discard)
    {
        uhd_error_code = uhd_rx_streamer_recv(rx_streamer, buffs_ptr, samps_per_buff, &md, 3.0, false, &num_rx_samps);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Stream receive)
        }
        uhd_rx_metadata_error_code(md, &md_error_code);
        if(md_error_code)
        {
            printf("Metadata error code: %d\n", md_error_code);
            uhd_rx_metadata_strerror(md, md_error_str, sizeof(md_error_str));
            printf("%s\n", md_error_str);
            return 1;
        }
        if(num_rx_samps != samps_per_buff)
        {
            printf("Receive number error!\n");
            return 1;
        }
        if((t_acc%10) == 0)
        {
            uhd_rx_metadata_time_spec(md, &full_secs, &frac_secs);
            printf("Receive data at %d, %.12f\n", (int)full_secs, frac_secs);
            fflush(stdout); //for matlab output
        }
        t_acc += 1;
    }
    t_acc = 0;

    while(t_acc < t_samples)
    {
        buffs_ptr[0] = buff_ch1[buff_head];
        buffs_ptr[1] = buff_ch2[buff_head];

        uhd_error_code = uhd_rx_streamer_recv(rx_streamer, buffs_ptr, samps_per_buff, &md, 1.0, false, &num_rx_samps);
        if(uhd_error_code)
        {
            DISPLAY_UHD_ERROR(Stream receive)
        }
        uhd_rx_metadata_error_code(md, &md_error_code);
        if(md_error_code)
        {
            printf("Metadata error code: %d\n", md_error_code);
            uhd_rx_metadata_strerror(md, md_error_str, sizeof(md_error_str));
            printf("%s\n", md_error_str);
            return 1;
        }
        if(num_rx_samps != samps_per_buff)
        {
            printf("Receive number error!\n");
            return 1;
        }
        if((t_acc%10) == 0)
        {
            uhd_rx_metadata_time_spec(md, &full_secs, &frac_secs);
            printf("Receive data %4d at %4d, %.12f\n", (int)(t_acc/10+1), (int)full_secs, frac_secs);
            fflush(stdout); //for matlab output
        }
        t_acc += 1;

        buff_head = (buff_head+1) % BUFF_LENGTH;
        sem_post(&sem);
    }

    stream_cmd.stream_mode = UHD_STREAM_MODE_STOP_CONTINUOUS;
    uhd_error_code = uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
    if(uhd_error_code)
    {
        DISPLAY_UHD_ERROR(Issue stream command)
    }
    printf("Finish.\n");

    //----20.Wait subthread finish
    int sem_value = 100;
    while(1)
    {
        Sleep(100);
        sem_getvalue(&sem, &sem_value);
        if(sem_value == -1)
        {
            pthread_cancel(pt);
            sem_destroy(&sem);
            break;
        }
    }

    //----21.Free
    if(file_path[0])
    {
        if(n_channels == 2)
        {
            fclose(fp_ch2);
            printf("Close ch2 file.\n");
        }
        {
            fclose(fp_ch1);
            printf("Close ch1 file.\n");
        }
    }
    uhd_rx_metadata_free(&md);
    uhd_rx_streamer_free(&rx_streamer);
    uhd_usrp_free(&usrp);
    for(int i=0; i<BUFF_LENGTH; i++)
    {
        free(buff_ch1[i]);
        free(buff_ch2[i]);
    }

    //system("pause");

    return 0;
}

void* write_data(void* arg)
{
    int n_channels;
    size_t samps_per_buff;

    n_channels = *(int*)arg;
    samps_per_buff = *((size_t*)arg+1);
    
    while(1)
    {
        sem_wait(&sem);

        if(buff_head == buff_tail)
            printf("Buff error!\n");
        else
        {
            if(file_path[0])
            {
                {
                    fwrite(buff_ch1[buff_tail], sizeof(int16_t)*2, samps_per_buff, fp_ch1);
                    fflush(fp_ch1);
                }
                if(n_channels == 2)
                {
                    fwrite(buff_ch2[buff_tail], sizeof(int16_t)*2, samps_per_buff, fp_ch2);
                    fflush(fp_ch2);
                }
            }
            buff_tail = (buff_tail+1) % BUFF_LENGTH;
        }
    }

    return NULL;
}
