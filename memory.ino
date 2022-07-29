#include <WiFi.h>

#define WIFI_NETWORK "ESP32"
#define WIFI_PASSWORD "genius122"
#define WIFI_TIMEOUT_MS 20000

#define MAX_DIFFICULTY 6

//Usaremos a porta 80
WiFiServer server(80);

//Os pinos dos leds usados
const int outputs[] = {25,26,27,33};
const int n_outputs = sizeof(outputs)/sizeof(const int);

//O vetor no qual serao guardadas as cores que devem ser apertadas (geracao pseudo-aleatoria)
int answers[MAX_DIFFICULTY];

//Quantas cores se deve responder em cada rodada e a quantidade de rodadas acertadas
int difficulty;

//Posicao da sequencia de cores que deve ser respondida agora
int current_answer = 0;

unsigned long start_time;






void restart_game(){
  difficulty = 1;
  current_answer = 0;
  for(int i = 0; i < MAX_DIFFICULTY; ++i){
    answers[i] = 0;
  }
  generate_answers();
  show_answers();
}

void generate_answers(){
  //Atribui um pino pseudo-aleatorio de outputs para cada item da sequencia answers, respeitando a dificuldade
  for(int i = 0; i < difficulty; ++i){
    answers[i] = outputs[random(0,n_outputs)];
    Serial.println(answers[i]);
  }
}

void show_answers(){
  for(int i = 0; i < difficulty; ++i){
    digitalWrite(answers[i], HIGH);
    delay(1000);
    digitalWrite(answers[i], LOW);
    delay(1000);
  }
}

void create_network(){
  Serial.println("Creating Network...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NETWORK, WIFI_PASSWORD);

  start_time = millis();

  server.begin();
  //Serial.println(WiFi.localIP());
}

void send_response(WiFiClient client){
  //Manda uma resposta para a requisicao feita pelo cliente
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
}

int analyze_request(String header){
  //Toma decisoes diferentes dependendo de qual "pagina" do site esta sendo requisitada
  if(header.indexOf("GET /restart") >= 0){
    restart_game();
    Serial.println("Dentro");
    return 0;
  }
  else if(header.indexOf("GET /game") >= 0){
    Serial.println("Parou?");
    return 0;
  }
  else if(header.indexOf("GET /pin") < 0){
    return 0;
  }

  //Pega a requisicao get do pino da resposta atual (answers[current_answer])
  char get_request[15] = "GET /pin/";
  char pin_name[4];
  sprintf(pin_name, "%d", answers[current_answer]);
  strcat(get_request, pin_name);
  Serial.println(get_request);

  //E compara pra ver se ela foi chamada
  if(header.indexOf(get_request) >= 0){
    //Vai pra proxima cor a ser respondida
    ++current_answer;
    
    //Se todas ja foram, sobe a dificuldade
    if(current_answer == difficulty){
      current_answer = 0;
      
      //Se a dificuldade ja era a maxima, o cliente ganhou
      if(difficulty == MAX_DIFFICULTY){
        return 1;
      }
      
      ++difficulty;
      generate_answers();
      show_answers();
    }
    return 0;
  }
  else{
    //Se o cliente errou
    return -1;
  }
}

void display_header(WiFiClient client){
    client.println(
    "<!DOCTYPE html><html>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta charset='utf-8'>"
    "<title>Jogo da Memoria</title>"
    "</head>"
    "<style>"
    ".center {"
    "margin: auto;"
    "width: 60%;"
    "padding:10px;}"
    ".button {"
    "/*background-color: #4CAF50; /* Green */*/"
    "border: none;"
    "color: white;"
    "padding: 15px 32px;"
    "text-align: center;"
    "text-decoration: none;"
    "display: inline-block;"
    "font-size: 16px;"
    "}</style>"
    );
}

void display_page(WiFiClient client){
  //Retorna a pagina HTML para ser mostrada ao cliente
  display_header(client);
  client.println(
    "<body> <div class='center'><h1> <a href='/game'>Teste sua Memoria! </a> </h1>"
    "</br>"
    "<a href='/restart' class='button' style='background-color: black;'>Reiniciar Jogo</a>"
    "</br>"
    "<a href='/pin/25' class='button' style='background-color: blue;'>Azul</a>"
    "</br>"
    "<a href='/pin/26' class='button' style='background-color: red;'>Vermelho</a>"
    "</br>"
    "<a href='/pin/27' class='button' style='background-color: green;'>Verde</a>"
    "</br>"
    "<a href='/pin/33' class='button' style='background-color: yellow;'>Amarelo</a>"
  );
  client.print(
    "<p>Pontos: "
   ); client.print(difficulty-1);
   client.println("</p></div></body></html>");
  client.println();
}

void display_win_page(WiFiClient client){
  display_header(client);
  client.println(
    "<body> <div class='center'> <p> **clap clap clap clap** </p>"
    "<a href='/restart'> Reiniciar Jogo</a> </div>"
    "</body></html>"
  );
  client.println();
}

void display_loss_page(WiFiClient client){
  display_header(client);
  client.println(
    "<body> <div class='center'> <p> **sniff sniff sniff sniff** </p>"
    "<a href='/restart'> Reiniciar Jogo</a> </div>"
    "</body></html>"
  );
  client.println(); 
}

void setup() {
  Serial.begin(115200);
  restart_game();
  for(int i = 0; i < n_outputs; ++i){
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i], LOW);  
  }
  create_network();
}

void loop() {
  //Espera por uma requisicao
  WiFiClient client = server.available();
  
  if(client){
    start_time = millis();
    unsigned long current_time = start_time;
    
    Serial.println("A request's here uWu!");

    //Buffer que guardara os caracteres da linha atual da requisicao
    String line_buffer = "";

    //String que guardara toda a requisicao
    String header = "";

    //Enquanto o cliente estiver conectado (e o tempo de espera eh menor que nosso timeout)
    while(client.connected() && (current_time - start_time) < WIFI_TIMEOUT_MS){ //Precisa limitar o numero de clientes para 1
      current_time = millis();
      
      if(client.available()){
        char c = client.read();
        Serial.write(c);
        header += c;

        if(c == '\n'){
          /*Uma requisicao HTTP acaba com \n\n
          * Como line_buffer eh zerado quando se encontra um \n (ver linhas abaixo), para que ele esteja zerado nesse ponto,
          * precisamos do caso em que foi encontrado \n\n
          */
          if(line_buffer.length() == 0){
            send_response(client);
            
            int result = analyze_request(header);
            if(result == 1){
              display_win_page(client);
              restart_game();
            }
            else if(result == -1){
              display_loss_page(client);
              restart_game();
            }
            else{
              display_page(client);
            }
            break;
          }
          //line_buffer eh zerado quando se acaba a linha (\n)
          line_buffer = "";
        }
        else if(c != '\r'){
          line_buffer += c;
        }
      }
    }
  }
}
