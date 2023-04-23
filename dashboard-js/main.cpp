#include <curl/curl.h>
#include <json/json.h>
#include <iostream>

// Uso: g++ main.cpp -std=c++20 -ljsoncpp -lcurl -o main
// Eu não precisei instalar nada, não sei se é porque eu já tinha instalado.

// Não perdi muito tempo tentando entender.
size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
   Json::Value *jsonData = static_cast<Json::Value *>(userdata);
   Json::Reader jsonReader;
   jsonReader.parse(ptr, *jsonData);
   return size * nmemb;
}

// Recebe os dados do servidor
Json::Value getData() {
   CURL *curl;
   CURLcode res;
   Json::Value jsonData;

   curl = curl_easy_init();
   if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3000/getData");
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jsonData);
      res = curl_easy_perform(curl);
      curl_easy_cleanup(curl);
   }
   return jsonData;
}

// Atualiza os dados do servidor
void updateData(Json::Value jsonData) {
   CURL *curl;
   CURLcode res;
   struct curl_slist *headers = NULL;
   std::string jsonString = jsonData.toStyledString();

   headers = curl_slist_append(headers, "Content-Type: application/json");
   curl = curl_easy_init();
   if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3000/updateData");
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonString.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      res = curl_easy_perform(curl);
      curl_easy_cleanup(curl);
   }
}

int main (int argc, char *argv[]) {
   Json::Value jsonData = getData();
   std::cout << jsonData << std::endl;
   updateData(jsonData);
   return 0;
}