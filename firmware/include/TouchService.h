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
