import math
import datetime

def calculate_new_position(lat, lon, distance, angle):
    """
    Calcule une nouvelle position GPS en utilisant une position initiale, une distance et un angle.
    
    Arguments:
    lat (float) : Latitude initiale en degrés.
    lon (float) : Longitude initiale en degrés.
    distance (float) : Distance à parcourir en mètres.
    angle (float) : Angle en degrés à partir du nord (0° = Nord, 90° = Est, 180° = Sud, 270° = Ouest).
    
    Retourne:
    new_lat (float) : Nouvelle latitude en degrés.
    new_lon (float) : Nouvelle longitude en degrés.
    """
    # Convertir la distance en km et l'angle en radians
    distance_km = distance / 1000.0
    angle_rad = math.radians(angle)

    # Rayon de la Terre en km
    R = 6371.0

    # Convertir latitude et longitude en radians
    lat_rad = math.radians(lat)
    lon_rad = math.radians(lon)

    # Calculer la nouvelle latitude
    new_lat_rad = math.asin(math.sin(lat_rad) * math.cos(distance_km / R) +
                            math.cos(lat_rad) * math.sin(distance_km / R) * math.cos(angle_rad))

    # Calculer la nouvelle longitude
    new_lon_rad = lon_rad + math.atan2(math.sin(angle_rad) * math.sin(distance_km / R) * math.cos(lat_rad),
                                       math.cos(distance_km / R) - math.sin(lat_rad) * math.sin(new_lat_rad))

    # Convertir la nouvelle latitude et longitude en degrés
    new_lat = math.degrees(new_lat_rad)
    new_lon = math.degrees(new_lon_rad)

    return new_lat, new_lon

def checksum(sentence):
    """
    Calcule et retourne le checksum d'une trame NMEA.
    
    Arguments:
    sentence (str) : Trame NMEA sans le caractère '$' initial ni le checksum final.
    
    Retourne:
    crc (int) : Checksum calculé pour la trame NMEA.
    """
    crc = 0
    for c in sentence:
        crc = crc ^ ord(c)
    crc = crc & 0xFF
    return crc

def gen_gga(parameters, hdop=6.0):
    """
    Génère une trame NMEA GGA (Global Positioning System Fix Data) à partir des paramètres fournis.

    Arguments:
    parameters (dict) : Dictionnaire contenant les informations suivantes:
        - "timestamp" : Heure actuelle sous forme de datetime.
        - "latitude" : Latitude en degrés.
        - "longitude" : Longitude en degrés.
    hdop (float) : Dilution horizontale de précision (HDOP), qui représente l'incertitude de la mesure.
                   Une valeur plus faible de HDOP indique une meilleure précision. La valeur par défaut est 1.0.

    Retourne:
    str : Trame NMEA GGA générée.
    """
    # Formater l'heure en UTC pour la trame NMEA
    now = parameters["timestamp"]
    timestamp = "{:.2f}".format(float(now.strftime("%H%M%S.%f")))

    # Formater la latitude pour la trame NMEA
    lat_abs = abs(parameters["latitude"])
    lat_deg = int(lat_abs)
    lat_min = (lat_abs - lat_deg) * 60
    lat_pole = 'S' if parameters["latitude"] < 0 else 'N'
    lat_format = f"{lat_deg:02}{lat_min:07.4f}"

    # Formater la longitude pour la trame NMEA
    lon_abs = abs(parameters["longitude"])
    lon_deg = int(lon_abs)
    lon_min = (lon_abs - lon_deg) * 60
    lon_pole = 'W' if parameters["longitude"] < 0 else 'E'
    lon_format = f"{lon_deg:03}{lon_min:07.4f}"

    # Construire la trame NMEA GGA avec le HDOP
    result = f"GPGGA,{timestamp},{lat_format},{lat_pole},{lon_format},{lon_pole},1,08,{hdop:.1f},0.0,M,0.0,M,,"
    crc = checksum(result)

    return f"${result}*{crc:02X}\r\n"