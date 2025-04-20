#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"

typedef struct WeatherData {
    int error_code;
    char weatherDesc[50];
    char temp_C[10];
    char windspeedKmph[10];
    char winddir16Point[10];
    char mintempC[10];
    char maxtempC[10];
} WeatherData;

struct memory
{
    char *response;
    size_t size;
};

static void init_memory(struct memory *chunk)
{
  chunk->response = malloc(1);  /* grown as needed with realloc */
  chunk->size = 0;            /* no data at this point */
}

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *clientp)
{
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)clientp;
    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if (!ptr)
    {
        /* out of memory! */
        printf("недостаточно памяти (realloc вернул NULL)\n");
        return 0;
    }

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

char *get_weather_data(const char *city) {
    CURL *handler;
    CURLcode res_code;

    struct memory chunk;
    init_memory(&chunk);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    handler = curl_easy_init();

    if (!handler) {
        printf("Ошибка инициализации libcurl\n");
        free(chunk.response);
        return NULL;
    }

    char url[256];
    snprintf(url, sizeof(url), "https://wttr.in/%s?format=j1", city);

    curl_easy_setopt(handler, CURLOPT_URL, url);
    curl_easy_setopt(handler, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(handler, CURLOPT_WRITEDATA, (void *)&chunk);

    res_code = curl_easy_perform(handler);

    if (res_code != CURLE_OK) {
        printf("Ошибка при запросе к API: %s\n", curl_easy_strerror(res_code));
        curl_easy_cleanup(handler);
        free(chunk.response);
        return NULL;
    }

    curl_easy_cleanup(handler);
    curl_global_cleanup();

    return chunk.response;
}


WeatherData parse_weather_data(const char *json_data) {
    
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        printf("Ошибка разбора JSON: %s\n", cJSON_GetErrorPtr());
        return (WeatherData){.error_code = 1};
    }

    cJSON *current_condition = cJSON_GetObjectItemCaseSensitive(root, "current_condition");
    if (!current_condition || !cJSON_IsArray(current_condition) || cJSON_GetArraySize(current_condition) == 0) {
        printf("Не удалось получить текущие погодные условия\n");
        cJSON_Delete(root);
        return (WeatherData){.error_code = 2};
    }

    WeatherData wd;

    cJSON *condition = cJSON_GetArrayItem(current_condition, 0);
    cJSON *temp_C = cJSON_GetObjectItemCaseSensitive(condition, "temp_C");
    cJSON *weatherDesc = cJSON_GetObjectItemCaseSensitive(condition, "weatherDesc");
    cJSON *windspeedKmph = cJSON_GetObjectItemCaseSensitive(condition, "windspeedKmph");
    cJSON *winddir16Point = cJSON_GetObjectItemCaseSensitive(condition, "winddir16Point");

    if (!cJSON_IsArray(weatherDesc) || cJSON_GetArraySize(weatherDesc) == 0) {
        printf("Не удалось получить описание погоды\n");
        cJSON_Delete(root);
        return (WeatherData){.error_code = 3};
    }

    cJSON *weatherDescItem = cJSON_GetArrayItem(weatherDesc, 0);
    cJSON *weatherDescValue = cJSON_GetObjectItemCaseSensitive(weatherDescItem, "value");

    strncpy(wd.weatherDesc, weatherDescValue ? weatherDescValue->valuestring : "N/A", sizeof(wd.weatherDesc) - 1);
    strncpy(wd.temp_C, temp_C ? temp_C->valuestring : "N/A", sizeof(wd.temp_C) - 1);
    strncpy(wd.windspeedKmph,  windspeedKmph ? windspeedKmph->valuestring : "N/A", sizeof(wd.windspeedKmph) - 1);
    strncpy(wd.winddir16Point, winddir16Point ? winddir16Point->valuestring : "N/A", sizeof(wd.winddir16Point) - 1);

    cJSON *weather = cJSON_GetObjectItemCaseSensitive(root, "weather");
    if (!weather || !cJSON_IsArray(weather) || cJSON_GetArraySize(weather) == 0) {
        printf("Не удалось получить прогноз погоды\n");
        cJSON_Delete(root);
        return (WeatherData){.error_code = 4};
    }

    cJSON *today = cJSON_GetArrayItem(weather, 0);
    cJSON *maxtempC = cJSON_GetObjectItemCaseSensitive(today, "maxtempC");
    cJSON *mintempC = cJSON_GetObjectItemCaseSensitive(today, "mintempC");

    strncpy(wd.maxtempC, maxtempC ? maxtempC->valuestring : "N/A", sizeof(wd.maxtempC) - 1);
    strncpy(wd.mintempC, mintempC ? mintempC->valuestring : "N/A", sizeof(wd.mintempC) - 1);
    
    cJSON_Delete(root);
    return wd;
}

void print_weather_data(const WeatherData *wd) {
    printf("Погода: %s\n", wd->weatherDesc);
    printf("Температура: %s°C\n", wd->temp_C);
    printf("Ветер: %s км/ч\n", wd->windspeedKmph);
    printf("Направление ветра: %s\n", wd->winddir16Point);
    printf("Диапазон температур:\n");
    printf(" - максимальная: %s°C\n", wd->maxtempC);
    printf(" - минимальная: %s°C\n", wd->mintempC);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Отсутствует обязательный аргумент\n");
        printf("Использование: %s <город>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *weather_data = get_weather_data(argv[1]);
    if (!weather_data) {
        printf("Не удалось получить данные о погоде для города: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    WeatherData wd = parse_weather_data(weather_data);
    if (wd.error_code != 0) {
        return EXIT_FAILURE;
    }
    print_weather_data(&wd);

    free(weather_data);

    return EXIT_SUCCESS;
}
