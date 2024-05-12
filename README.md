# AASun : Multi channel solar diverter / Routeur solaire multi voies

_The project is documented in French because it was designed according to the characteristics of the Linky meter which is specifically French. The router can, however, be used directly on any 50Hz electrical network._

## Pourquoi un routeur?
Une installation solaire peut être utilisée en « Auto Consommation », c’est à dire que l’électricité produite est utilisée localement par le producteur plutôt qu’être vendue.
Sans équipement spécialisé la consommation est aléatoire :
- Une partie de la production est utilisée pour le « talon » de consommation du logement : réfrigérateur, VMC, circulateur du chauffage central…
- A certains moments plusieurs consommateurs peuvent s’activer en même temps et le total de leur consommation dépasser la production photovoltaïque.
- A d’autres moments il y a peu de consommateurs et le surplus de production photovoltaïque est injecté sur le réseau public (gratuitement).

Pour gérer au mieux l’électricité photovoltaïque produite on peut utiliser un routeur d’énergie. Ce routeur va diriger l’énergie disponible vers plusieurs équipements en se basant sur les mesures des énergies consommée et produites, de l’environnement (capteurs de températures, contact de thermostat…), et de règles définies par l’utilisateur.

Après avoir installé quelques panneaux solaires, j’ai décidé de réaliser mon routeur solaire basé sur un STM32G071CB.
La genèse de ce routeur, sa configuration et son utilisation sont expliquées dans le document `Doc/AASun.pdf`.

## Principales caractéristiques :
-	Prévu pour être placé sur un support pour rail DIN
-	Microcontrôleur STM32G071CB et flash externe W25Q64
-	Afficheur 1.3’’ et 2 boutons pour naviguer dans les pages (l'afficheur de 0.96'' est aussi géré)
-	Interface réseau filaire
-	Interface WIFI
-	Serveur HTTP pour le monitoring, la configuration du routeur et des règles de routage/forçage, l’affichage des historiques.
-	2 ou 4 capteurs de courant (CT).
-	2 sorties PWM de routage pour relais statique (SSR).
-	1 relais 230V 3A sur la carte.
-	1 sortie auxiliaire pouvant piloter un autre relais.
-	2 entrées logiques (thermostat, bouton poussoir, heures pleines/creuses, …).
-	2 entrées de comptage (jusqu’à 30V). Si ces entrées ne sont pas utilisées en comptages elles peuvent être utilisées en entrée logiques.
-	4 capteurs de température DS18B20.
-	2 règles de routages pour les sorties PWM.
-	8 forçages comportant chacun une règle de démarrage et une règle d’arrêt pour piloter les 4 sorties du routeur.
-	Gestion de priorité des routages et des forçages.
-	Plusieurs modes de forçage : PWM, standard, burst.
-	Historique des énergies journalières sur 31 jours. Historique des puissances moyennées par ¼ heure sur 31 jours. Les 9 valeurs prises en compte sont : CT1 à CT4 (CT1 sépare le soutirage et l’injection),  routage estimé des sorties PWM 1 et 2, compteurs d’impulsion 1 et 2.
-	Gestion anti légionellose.
-	Interface Linky.
-	Interface console par UART ou Telnet (réseau filaire ou WIFI).

![AASun_IO](https://github.com/Nikitarc/AASun/assets/101801098/16495067-dde8-47d1-9afe-b8b53f5aedf1)


## Les dossiers
`Doc` contient la documentation du routeur

`AASUN` est un projet utilisable avec l’environnement de développement STM32CubeIDE pour générer le code du routeur.

`mfs/exec_Win32/AASun_web` contient les fichiers du serveur HTTP. Ce dossier est transformé en `AASun_web.bin` avec `mfsbuild`. `AASun_web.bin` est le système de fichier MFS à placer dans la flash externe du routeur.

`mfs` est un projet Visual Studio 2022 qui permet de générer l’application `mfsbuild` qui permet de créer `AASun_web.bin`. Il permet aussi de générer l’application `SerEl` qui permet de flasher ce système de fichier dans la flash externe du routeur par l’intermédiaire d’un UART.

`AASun_External_Loader` est un projet STM32CubeIDE qui génère un « External Loader » utilisable avec STM32CubeProgrammer. Cela permet de téléverser `AASun_web.bin` dans la flash externe du routeur, en utilisant une sonde ST-LINK.

`DipTrace` contient les schémas électroniques et les fichiers pour la création des circuits imprimés. Réalisés avec DipTrace : https://diptrace.com/fr/.

`http` contient le projet Espressif-IDE pour générer l'application du module WIFI "ESP32-C3 Super Mini"

Tous les fichiers du routeur sont fournis sous une forme directement utilisable : Il suffit d’avoir STM32CubeProgrammer et un convertisseur USB/UART pour mettre à jour le routeur.
Le logiciel du module WIFI doit être compilé avec Espressif-IDE, qui sert aussi à la téléverser.
