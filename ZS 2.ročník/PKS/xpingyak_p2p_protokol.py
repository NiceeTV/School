import socket
import threading
import time
import tkinter as tk
from tkinter import filedialog
import os

MAX_FRAGMENT_SIZE = 1463 #daná protokolom

class Peer:
    def __init__(self,ip,port,server_ip,server_port,fragment_size) -> None:
        self.sender_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listen_sock.settimeout(36) #po 36s prestane prijímať pakety a považuje pokus o komunikáciu neúspešný
        self.listen_sock.bind((ip,port))

        self.server_ip = server_ip
        self.server_port = server_port
        self.max_fragment_size = fragment_size

        self.sqn = 0
        self.ack = 0
        self.expected_sqn = 0
        self.last_sent_paket = None #posledne poslaný paket, pre prípad straty
        self.print_lock = threading.Lock()

        self.connected = False #sme pripojený?
        self.accepted = False #prijatie handshaku druhou stranou
        self.other_closed = False #druhý peer ukončil svoje posielanie
        self.me_closed = False #ja som prerušil posielanie
        self.ready = True #je možný výpis
        self.exit = False #ukončenie programu

        #musí si otvoriť porty na počúvanie
        self.listener_thread = threading.Thread(target=self.receive)
        self.listener_thread.start()

        #vlákno na vstup
        self.sender_thread = threading.Thread(target=self.interface)
        self.sender_thread.start()

        #vlákna na otvorenie spojenia
        self.establish_thread = threading.Thread(target=self.establish_connection)
        self.establish_thread.start()

        self.listener_thread.join()  # Čakáme, kým sa listener vlákno dokončí
        self.sender_thread.join()  # Čakáme, kým sa sender vlákno dokončí
        self.establish_thread.join()

        print("program skoncil")



    def establish_connection(self):
        print("Establishing connection ...")

        count = 0
        while not self.accepted: #posielaj každých 6s SYN paket
            self.sqn = 0
            self.ack = 0

            #syn_packet = self.create_packet("", packet_id, 0x00, self.sqn,self.ack, 1)
            syn_packet = self.create_packet("",0x00,self.sqn,self.ack)
            self.sender_socket.sendto(syn_packet, (self.server_ip, self.server_port))

            with self.print_lock:
                print(f"Odoslaný SYN: SQN={self.sqn}, ACK={self.ack}")
            time.sleep(6)
            count += 1

            if count == 6: #36s a ukončí posielanie sám
                print("Spojenie sa nepodarilo, druhá strana neodpovedá.")
                self.exit = True
                self.sender_socket.close()
                break




    def interface(self):
        while not self.exit:
            if self.connected:
                if self.ready:
                    choice = input("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")
                    if choice == '1':
                        self.change_fragment_size()

                        message = input("Zadajte správu: ") #výzva na zadanie správy
                        self.send_message(message)
                    elif choice == '2':
                        self.change_fragment_size()

                        #výzva na výber súboru
                        print("Tento program zatiaľ nepodporuje túto službu.")
                        #file_path = input("Zadajte cestu k súboru: ")
                        #self.send_file(file_path)

                    elif choice == '3': #poslanie zlého paketu
                        self.change_fragment_size()

                        message = input("Zadajte správu: ") #výzva na zadanie správy
                        self.send_false_packet(message)

                    elif choice.lower() == 'q':
                        #termination TO-DO
                        self.terminate_connection()
                        break
                    else:
                        print("Neplatná voľba, skúste znova.")



    def change_fragment_size(self):
        valid = False
        choice = None

        while not valid:
            with self.print_lock:
                choice = input("Chcete zmenit maximálnu veľkosť fragmentov? y/n ")

            if choice == 'y' or choice == 'Y' or choice == 'n' or choice == 'N':
                valid = True


        if choice == 'y' or choice == 'Y':  # výzva na zadanie veľkosti
            fragment_size = 0
            while True:
                print("Zadajte maximálnu veľkosť fragmentu: ")
                try:
                    fragment_size = int(input())
                except ValueError:
                    continue

                if 0 < max_fragment_size <= MAX_FRAGMENT_SIZE: #max limit fragmentu pre moju hlavičku
                    break

            self.max_fragment_size = fragment_size


    def send_message(self,message):
        self.sqn += len(message)
        self.last_sent_paket = self.create_packet(bytes(message,encoding='utf-8'),0x04,self.sqn,self.ack,length=len(message))
        self.expected_sqn = self.sqn

        self.sender_socket.sendto(self.last_sent_paket, (target_ip, sending_port))
        with self.print_lock:
            self.ready = False
            print(f"Odoslaná správa: {message}")


    def send_false_packet(self,message):
        self.sqn += len(message)
        self.last_sent_paket = self.create_packet(bytes(message, encoding='utf-8'),  0x04, self.sqn, self.ack, length=len(message))
        self.expected_sqn = self.sqn
        false_packet = self.last_sent_paket.copy() #kopia paketu

        #vytvoríme chybu, pridáme zlé crc
        false_packet[7] = 0xFF
        false_packet[8] = 0xFF

        self.sender_socket.sendto(false_packet, (target_ip, sending_port))
        with self.print_lock:
            self.ready = False
            print(f"Odoslaná správa: {message}")


    def send_file(self,file_path): #to-do
        pass


    def receive(self):
        while not self.exit:
            try:
                response, address = self.listen_sock.recvfrom(MAX_FRAGMENT_SIZE)
                sqn = (response[1] << 8) | response[2]
                ack = (response[3] << 8) | response[4]
            except socket.timeout:
                print("Čas na prijatie komunikačného paketu vypršal. Ukončujem program.")
                break

            if not self.connected:
                if response[0] == 0x00: #ak dostane SYN, prestane posielať svoj SYN a zahaji handshake
                    self.accepted = True

                    #idem poslať SYN-ACK
                    self.ack = sqn + 1
                    syn_ack_packet = self.create_packet(bytes("", encoding='utf-8'),  0x01, self.sqn, self.ack)
                    self.sender_socket.sendto(syn_ack_packet, (self.server_ip, self.server_port))
                    with self.print_lock:
                        print(f"Prijatý SYN: SQN={sqn}, ACK={ack}")
                        print(f"Odoslaný SYN-ACK: SQN={self.sqn}, ACK={self.ack}")


                if response[0] == 0x01: #SYN-ACK, pošli zase ACK
                    self.sqn = ack
                    self.ack = sqn + 1

                    ack_packet = self.create_packet(bytes("", encoding='utf-8'), 0x02, self.sqn, self.ack)
                    self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))
                    with self.print_lock:
                        print(f"Prijatý SYN-ACK: SQN={sqn}, ACK={ack}")
                        print(f"Odoslaný ACK: SQN={self.sqn}, ACK={self.ack}")

                    #zahájena komunikácia
                    self.accepted = True
                    self.connected = True
                    print("Zacala komunikacia")

                if response[0] == 0x02:
                    self.connected = True

                    print(f"Prijatý ACK: SQN={sqn}, ACK={ack}")
                    print("Zacala komunikacia")


            if self.connected: #uz komunikacia s datami/subormi
                self.ack = (response[1] << 8) | response[2]
                self.sqn = (response[3] << 8) | response[4]

                if response[0] == 0x06: #FIN paket, pošlem späť ACK paketň
                    ack_packet = self.create_packet(bytes("", encoding='utf-8'), 0x02, self.sqn, self.ack)
                    self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))
                    #peer2 sa rozhodol, že už nebude posielať pakety, ale my ešte môžeme posielať jemu

                    if self.me_closed:
                        #koniec
                        self.listen_sock.close()
                        print("Koniec komunikácie.")
                        self.ready = True
                        break
                    else: #ešte nekončí komunikácia, čaká sa na moj FIN
                        with self.print_lock:
                            print(f"\nPeer ({target_ip}) už nebude posielať žiadne pakety.")
                            print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

                        self.other_closed = True


                if response[0] == 0x04: #to je správa, nefragmentovaná
                    length = (response[5] << 8) | response[6]
                    print(length)
                    sprava = response[9:9+length]
                    with self.print_lock:
                        print(f"\nPrijatá správa: {sprava.decode()} (SQN={self.sqn}, ACK={self.ack})")
                        self.ready = True


                    #kontrola checksum
                    crc = (response[7] << 8) | response[8]

                    new_crc = self.crc16(sprava)
                    print(crc,new_crc)
                    if new_crc == crc:
                        with self.print_lock:
                            print("Paket je korektný.")
                            if not self.me_closed:
                                print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

                        #pošleme ACK paket ako potvrdenie
                        ack_packet = self.create_packet(bytes("", encoding='utf-8'), 0x02, self.sqn, self.ack)
                        self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))

                    else:
                        with self.print_lock:
                            print("Paket je poškodený.")

                        if self.last_sent_paket is not None:
                            last_packet_length = (self.last_sent_paket[7] << 8) | self.last_sent_paket[8]
                            self.ack -= last_packet_length #musíme sa vrátiť na posledný správne odoslaný paket

                        #pošleme NACK paket, vyžiadame si jeho opatovne poslanie
                        nack_packet = self.create_packet(bytes("", encoding='utf-8'), 0x03, self.sqn, self.ack)
                        self.sender_socket.sendto(nack_packet, (self.server_ip, self.server_port))

                if response[0] == 0x03:  # NACK, pošli ešte raz paket
                    self.sender_socket.sendto(self.last_sent_paket, (target_ip, sending_port))
                    with self.print_lock:
                        print("Poškodený paket. Posielam znova.")

                if response[0] == 0x02: #ACK, že prišla správa

                    if ack == self.expected_sqn: #prišlo všetko v poriadku
                        with self.print_lock:
                            print("Správa prišla prijímateľovi správne.")
                            self.ready = True

                    if self.other_closed:
                        #koniec spojenia
                        self.listen_sock.close()
                        print("Koniec komunikácie.")
                        break

        self.listen_sock.close()


    def terminate_connection(self):
        self.sqn += 1
        self.expected_sqn = self.sqn

        fin_packet = self.create_packet(bytes("", encoding='utf-8'), 0x06, self.sqn, self.ack)
        self.sender_socket.sendto(fin_packet, (self.server_ip, self.server_port))
        self.me_closed = True
        print("Ukončili ste posielanie paketov.")

    def create_packet(self,data: bytes,typ,sqn,ack,length=0,name_length=0,name="",file_size=0,fragment_size=1463):
        length = len(data)

        crc = self.crc16(data)
        header = bytearray()

        header.append(typ) #typ paketu

        #sqn
        header.append((sqn >> 8) & 0xFF)  #prvý bajt
        header.append(sqn & 0xFF)   #druhý bajt

        #ack
        header.append((ack >> 8) & 0xFF)
        header.append(ack & 0xFF)

        #tu sa rozdeluje hlavička dynamicky

        if length != 0: #ide o normálny paket
            # length v bajtoch je na 2 bajty uložená
            header.append((length >> 8) & 0xFF)
            header.append(length & 0xFF)




        #tu to môže byť réžijný alebo inicializačný (prenos súboru) paket
        if name_length != 0: #inicializacny paket
            #name length, dĺžka názvu súboru
            header.append((name_length >> 8) & 0xFF)
            header.append(name_length & 0xFF)

            #názov súboru
            header.extend(bytes(name,encoding='utf-8'))

            #file size
            header.append((file_size >> 24) & 0xFF)  # Najvyšší bajt
            header.append((file_size >> 16) & 0xFF)
            header.append((file_size >> 8) & 0xFF)
            header.append(file_size & 0xFF)

            #fragment size
            header.append((fragment_size >> 8) & 0xFF)
            header.append(fragment_size & 0xFF)


        #ak nemáme name_length, tak ide o rezijnu správu
        packet = header

        # crc16
        packet.append((crc >> 8) & 0xFF)
        packet.append(crc & 0xFF)
        #dáta/payload
        packet.extend(data)
        return packet

    def crc16(self,data: bytes) -> int:  #CCITT-FALSE
        INITIAL_VALUE = 0xFFFF
        POLYNOMIAL = 0x1021

        crc = INITIAL_VALUE  #inicializácia CRC na počiatočnú hodnotu
        for byte in data:
            crc ^= (byte << 8)  #XOR s aktuálnym bajtom (posun doľava)
            for _ in range(8):  #pre každý bit v bajte
                if (crc & 0x8000):  #ak je najvyšší bit 1
                    crc = (crc << 1) ^ POLYNOMIAL  #posun doľava a XOR s polynomom
                else:
                    crc <<= 1  #len posun doľava
                crc &= 0xFFFF  #obmedzenie na 16 bitov
        return crc ^ 0x0000


#nastavenie parametrov
listening_port = 0
sending_port = 0
target_ip = 0
max_fragment_size = 0


while True:
    print("Zadajte port na počúvanie: ")
    try:
        listening_port = int(input())
    except ValueError:
        continue
    break

while True:
    print("Zadajte cieľovú IP adresu: ")

    target_ip = input()
    l = target_ip.split(".")

    valid = True
    if len(l) == 4: #musia byť 4 čísla
        for c in l:
            if int(c) < 0 or int(c) > 255: #neplatný rozsah
                valid = False
                break

        if valid: #platná ip adresa
           break

while True:
    print("Zadajte port na posielanie: ")
    try:
        sending_port = int(input())
    except ValueError:
        continue
    break

while True:
    print("Zadajte maximálnu veľkosť fragmentu: ")
    try:
        max_fragment_size = int(input())
    except ValueError:
        continue

    if 0 < max_fragment_size <= MAX_FRAGMENT_SIZE:
        break

#TOTO TREBA ZMENIT PRED ODOVZDANIM
CLIENT_IP = "192.168.0.105" #naša IP


peer = Peer(CLIENT_IP,listening_port,target_ip,sending_port,max_fragment_size)
#to do - prerušiť program pri timeoute
