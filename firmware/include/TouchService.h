#pragma once

#include <Arduino.h>

// Initialise le contrôleur tactile XPT2046 sur le bus HSPI de la CYD.
// rotation doit correspondre à la rotation de l'écran (0-3).
void touchServiceBegin(uint8_t rotation);

// Met à jour la rotation (à appeler à chaque changement d'orientation).
void touchServiceSetRotation(uint8_t rotation);

// Lit la position tactile et la convertit en coordonnées écran.
// Retourne true si l'écran est touché, false sinon.
bool touchServiceRead(int16_t displayWidth, int16_t displayHeight, int16_t& x,
                      int16_t& y);

// Indique si l'écran est actuellement touché (lecture brute de l'IRQ, sans SPI).
// Peu coûteux: utilisé chaque boucle pour réinitialiser le minuteur d'inactivité
// ou réveiller l'écran.
bool touchServiceTouched();
