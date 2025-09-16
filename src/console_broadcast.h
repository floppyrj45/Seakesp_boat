#pragma once
#include <Arduino.h>

// Démarre le serveur TCP (port 10110)
void consoleBegin();

// Envoie une ligne (sans CRLF) vers le client TCP "console" si connecté
void consoleBroadcastLine(const String& line);

// Traite les données entrantes du client (à appeler dans loop)
void consoleLoop();

// À appeler dans loop(): maintient l'acceptation du client TCP
void consoleLoop();


