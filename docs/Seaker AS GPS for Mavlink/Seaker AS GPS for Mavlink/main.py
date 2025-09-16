import socket
import time
import datetime
from seaker import SeakerUDPConnection, start_connect_to_udp, send_ping
from generate_GGA import calculate_new_position, gen_gga
from datetime import datetime

def print_with_timestamp(message):
    """Imprime un message avec un horodatage."""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    print(f"[{timestamp}] {message}")

# Position GPS initiale (par exemple, Paris)
latitude_initiale = 47.6588  # Latitude de Vannes
longitude_initiale = -2.7609  # Longitude de Vannes
Soustraction_dist_Seaker = 600

# Configuration du socket UDP pour envoyer des données NMEA
UDP_IP = "192.168.0.148"  # Adresse IP de BlueOS
UDP_PORT = 27000  # Port du socket NMEA
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def main():
    # Démarrer la connexion UDP
    conn = start_connect_to_udp()

    # Attendre un instant pour établir la connexion
    time.sleep(2)

    # Envoyer la commande de ping
    send_ping(conn)

    # Boucle pour vérifier le statut et générer les trames GGA
    while True:

        # Lire les données de distance et d'angle
        angle = conn.get_angle()
        distance = conn.get_distance_m() - Soustraction_dist_Seaker

        # Vérifier si de nouvelles données sont disponibles
        if angle is not None and distance is not None:
            print_with_timestamp(f"Angle extrait: {angle:.2f}°, Distance extraite: {distance:.2f}m")

            # Calculer la nouvelle position
            nouvelle_latitude, nouvelle_longitude = calculate_new_position(
                latitude_initiale, longitude_initiale, distance, angle
            )

            # Créer les paramètres pour générer la trame NMEA
            params = {
                "timestamp": datetime.utcnow(),
                "latitude": nouvelle_latitude,
                "longitude": nouvelle_longitude,
            }

            # Générer la trame NMEA GGA
            trame_nmea = gen_gga(params)
            print_with_timestamp(f"Trame NMEA générée : {trame_nmea}")

            # Envoyer la trame NMEA au socket UDP de BlueOS
            sock.sendto(trame_nmea.encode('utf-8'), (UDP_IP, UDP_PORT))
            print_with_timestamp("Trame NMEA envoyée à BlueOS.")

            # Réinitialiser les valeurs pour la prochaine lecture après la génération de la trame
            conn.reset_values()

        else:
            continue
            #print_with_timestamp("Aucune nouvelle donnée reçue.")

        time.sleep(0.2)  # Pause avant la prochaine vérification

if __name__ == "__main__":
    main()
