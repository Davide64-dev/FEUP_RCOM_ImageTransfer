#include "data_link.h"
#include "statemachine.h"

volatile int STOP = FALSE;

int llOpenTransmiter(struct linkLayer* ll){

    printf("llOpen Transmiter called\n");
    int fd = open(ll->port, O_RDWR | O_NOCTTY);
    unsigned char buf[SET_SIZE] = {FLAG, A_SENDER, C_SET, A_SENDER ^ C_SET, FLAG};
    int finish = FALSE;

    while(finish == FALSE){
        int bytes = write(fd, buf, SET_SIZE);
        int bytes1 = read(fd, buf, SET_SIZE);
        buf[bytes1] = '\0';
        if (buf[0] == 'z')
            STOP = TRUE;
        alarm(3);

        if (bytes1 >= 0){
            alarm(0);
            finish = TRUE;
            printf("%d\n", bytes);
            printf("Transmiter Opened with success\n");
            return bytes;
        }
    }
    return 0;
}

int llOpenReceiver(struct linkLayer* ll){
    int fd = open(ll->port, O_RDWR | O_NOCTTY);
    unsigned char buf[SET_SIZE];

    state_machine* st = (state_machine*)malloc(sizeof(state_machine));
    st->adressByte = A_SENDER;

    while (STOP == FALSE)
    {
        st->current_state = START;
        int bytes = read(fd, buf, SET_SIZE);
        buf[bytes] = '\0';
        transition(st, buf, bytes, A_SENDER, C_SET);
        if (st->current_state == STATE_STOP){
            unsigned char buf[SET_SIZE] = {FLAG, A_RECEIVER, C_UA, A_RECEIVER ^ C_UA, FLAG};
            write(fd, buf, SET_SIZE);
            STOP = TRUE;
        }
        else{
            printf("wrong package\n");
        }
    }

    free(st);
    printf("Receiver Opened with success\n");
    return 0;
}

int llopen(struct linkLayer* ll, int mode){
    if (mode == TRANSMITER){
        return llOpenTransmiter(ll);
    }
    else if (mode == RECEIVER){
        return llOpenReceiver(ll);
    }
    return 0;
}

unsigned char BCC2(unsigned char* frame, int len){
    unsigned char res = frame[0];
    for (int i = 1; i < len; i++){
        res = res ^frame[i];
    }
    return res;
}

int byteStuffing(unsigned char* frame, int length) {

  unsigned char aux[length + 6];

  for(int i = 0; i < length + 6 ; i++){
    aux[i] = frame[i];
  }
  
  int finalLength = 4;
  for(int i = 4; i < (length + 6); i++){

    if(aux[i] == FLAG && i != (length + 5)) {
      frame[finalLength] = 0x7D;
      frame[finalLength+1] = 0x5E;
      finalLength = finalLength + 2;
    }
    else if(aux[i] == 0x7D && i != (length + 5)) {
      frame[finalLength] = 0x7D;
      frame[finalLength+1] = 0x5E;
      finalLength = finalLength + 2;
    }
    else{
      frame[finalLength] = aux[i];
      finalLength++;
    }
  }

  return finalLength;
}



int llwrite(struct linkLayer* li, unsigned char* frame, int length){
    int fd = open(li->port, O_RDWR | O_NOCTTY);

    unsigned char buf[length + 6];
    buf[0] = FLAG;
    buf[1] = A_SENDER;

    if (li->sequenceNumber == 0) buf[2] = 0;
    else buf[2] = 0x40;

    buf[3] = A_SENDER ^ buf[2];

    for (int i = 4; i <= length + 3;i++)
        buf[i] = frame[i-4];

    buf[length + 4] = BCC2(frame, length);

    printf("%c\n", (char)BCC2(frame, length));

    buf[length + 5] = FLAG;

    length = byteStuffing(buf, length);


    int bytes = write(fd, buf, length);

    printf("Sent frame: %d bytes\n", bytes);

    int finish = FALSE;

    unsigned char answer[5];

    state_machine* st = (state_machine*)malloc(sizeof(state_machine));

    while (!finish)
    {
        read(fd, answer, 5);

        if (li->sequenceNumber == 0){
            transition(st, answer, 5, A_RECEIVER, REJ0);
            if (st->current_state == STATE_STOP){
                write(fd, buf, length);
            }

            else{
                st->current_state = START;
                transition(st, answer, 5, A_RECEIVER, RR1);
                if(st->current_state == STATE_STOP){
                    alarm(0);
                    finish = TRUE;
                    printf("Mensagem recebida\n");
                    return bytes;
                }
            }
        }

        else if (li->sequenceNumber == 1){
            transition(st, answer, 5, A_RECEIVER, REJ1);

            if (st->current_state == STATE_STOP){
                write(fd, buf, length);
            }

            else{
                st->current_state = START;
                transition(st, answer, 5, A_RECEIVER, RR0);
                if(st->current_state == STATE_STOP){
                    alarm(0);
                    printf("mensagem recebida\n");
                    finish = TRUE;
                }
            }
        }

    }

    free(st);
    return 0;
}

int byteDestuffing(unsigned char* frame, int length) {

  unsigned char aux[length + 5];

  for(int i = 0; i < (length + 5) ; i++) {
    aux[i] = frame[i];
  }

  int finalLength = 4;

  for(int i = 4; i < (length + 5); i++) {

    if(aux[i] == 0x7D){
      if (aux[i+1] == 0x5D) {
        frame[finalLength] = 0x7D;
      }
      else if(aux[i+1] == 0x5D) {
        frame[finalLength] = FLAG;
      }
      i++;
      finalLength++;
    }
    else{
      frame[finalLength] = aux[i];
      finalLength++;
    }
  }

  return finalLength;
}

int llread(struct linkLayer* li, unsigned char* res){
    int fd = open(li->port, O_RDWR | O_NOCTTY);
    unsigned char buf[MAX_SIZE];
    int finish = FALSE;
    while (finish == FALSE)
    {
        int bytes = read(fd, buf, MAX_SIZE);

        if (bytes > -1){
            printf("Read %s with success: %d\n", buf, bytes);
        }
        byteDestuffing(buf, bytes);
        state_machine* st = (state_machine*)malloc(sizeof(state_machine));

        for (int i = 0; i < bytes; i++) printf("%d,", buf[i]);

        printf("\n");

        int controlByteRead;
        if (buf[2] == 0)
            controlByteRead = 0;
        else if (buf[2] == 0x40)
            controlByteRead = 1;

        transition(st, buf, 4, A_SENDER, controlByteRead);
        if (st->current_state == BCC_OK){
            free(st);

            if ((buf[bytes - 2] == BCC2(&buf[4], bytes - 6)) && buf[bytes-1] == FLAG){
                printf("BCC2 and the FLAG are ok\n");
                unsigned char answer[6];
                answer[0] = FLAG;
                answer[1] = A_RECEIVER;
                if (controlByteRead) answer[2] = RR0;
                else answer[2] = RR1;
                answer[3] = answer[1] ^ answer[2];
                answer[4] = FLAG;

                write(fd, answer, 5);

                printf("sent the answer\n");
                //char* res1 = (char *)malloc(bytes-5);
                //memcpy(res1, &buf[4], bytes - 6);
                //res = res1;
                //finish = TRUE;
                //free(res);
                //free(res1);
                return 0;
                
            }
            else{
                // send the rejection
                printf("wrong!\n");
                return -1;
            }

        }
        else{
            // send the rejection
            printf("wrong!\n");
            return -1;
        }

    }
    return 0;
}


/*
int main(){
    struct linkLayer* fd;
    fd = (struct linkLayer*)malloc(sizeof(struct linkLayer));
    strcpy(fd->port, "/dev/ttyS11");
    llopen(fd, RECEIVER);
    //llread(fd);
    free(fd);
    return 0;
}
*/



