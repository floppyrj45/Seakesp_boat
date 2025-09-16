# seaker.py
import threading
import socket
import time

from datetime import datetime

def print_with_timestamp(message):
    """Imprime un message avec un horodatage."""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    print(f"[{timestamp}] {message}")



class SeakerUDPConnection:
    def __init__(self, ip='192.168.0.148', port=15000):
        self.ip = ip
        self.port = port
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.client_socket.settimeout(1)  # Timeout de 1 seconde pour la réception
        self.distance_m = None
        self.angle = None
        self.running = True
        self.status = None
        self.lock = threading.Lock()  # Ajouter un verrou

    def connect(self):
        try:
            print(f"Tentative de connexion au serveur UDP {self.ip}:{self.port}...")
            # Envoyer une trame de test pour vérifier la connexion
            self.send("$REQ_CONFIG,\r\n")
            print("Envoi de la trame de test $REQ_CONFIG pour vérifier la connexion.")
            self.listen()  # Commencer à écouter les messages du serveur
        except Exception as e:
            print(f"Erreur lors de la connexion UDP : {e}")

    def send(self, message):
        try:
            self.client_socket.sendto(message.encode('utf-8'), (self.ip, self.port))
            print(f"Message envoyé : {message.strip()}")
        except Exception as e:
            print(f"Erreur d'envoi : {e}")

    def listen(self):
        while self.running:
            try:
                data, _ = self.client_socket.recvfrom(4096)  # Taille du buffer de réception
                self.process_data(data.decode('utf-8').strip())
            except socket.timeout:
                self.reset_values()  # Réinitialiser les valeurs seulement en cas de timeout
                #print("Aucun message reçu, réinitialisation des valeurs.")
            except Exception as e:
                print(f"Erreur de réception : {e}")
                break

    def process_data(self, data):
        #print(f"Seaker message: {data}")
        data = data.strip()

        if '$STATUS,0' in data:
            self.status = 0
            print("Seaker status 0 detected - Starting Seaker")
            self.start_seaker()
        elif '$STATUS,2' in data:
            self.status = 2
            #print("Seaker status 2 detected - Doing nothing")
        elif '$STATUS,1' in data:
            self.status = 1
            print("Seaker status 1 detected - Doing nothing")
        elif '$STATUS,3' in data:
            self.status = 3
            print("Seaker status 3 detected - Restarting Seaker")
            self.restart_seaker()
        elif '$DTPING,' in data:
            #print("DTPING message detected - Handling DTPING")
            self.handle_dtping(data)
        else:
            print(f"Message non reconnu : {data}")

    def start_seaker(self):
        print('Statut 0 reçu, lancement de la fonction Start_seaker.')
        self.send('$GOSEAK,4,1,1500,1*24\r\n')

    def restart_seaker(self):
        print('Statut 3 reçu, lancement de la fonction NoSync.')
        self.send('$GOSEAK,4,1,1500,1*24*20\r\n')

    def handle_dtping(self, data):
        try:
            elements = data.split(',')
            with self.lock:  # Protéger l'accès aux variables partagées
                self.angle = float(elements[3])
                distance_dm = float(elements[4])
                self.distance_m = (distance_dm / 10)
            #print(f"Angle: {self.angle:.0f}°, Distance: {self.distance_m:.1f}m")
        except (IndexError, ValueError) as e:
            print(f"Erreur de traitement du message DTPING : {e}")

    def get_angle(self):
        with self.lock:  # Protéger l'accès aux variables partagées
            #print(f"Angle actuel (get_angle): {self.angle}")
            return self.angle

    def get_distance_m(self):
        with self.lock:  # Protéger l'accès aux variables partagées
            #print(f"Distance actuelle (get_distance_m): {self.distance_m}")
            return self.distance_m

    def reset_values(self):
        """Réinitialiser l'angle et la distance à None uniquement en cas d'absence de données."""
        self.angle = None
        self.distance_m = None

    def get_status(self):
        return self.status

    def stop(self):
        self.running = False
        self.client_socket.close()
        print("Connexion fermée.")

def start_connect_to_udp():
    conn = SeakerUDPConnection(ip='192.168.0.148', port=15000)
    thread = threading.Thread(target=conn.connect)
    thread.daemon = True
    thread.start()
    return conn

def send_ping(conn):
    command = "$CONFIG,1,1,1500,0,0,0,0,0,2,1,1,1,0,0,0,0,0,0,0*21\r\n"
    conn.send(command)
    print("Commande envoyée : ", command)

if __name__ == "__main__":
    conn = start_connect_to_udp()
    time.sleep(1)
    send_ping(conn)
