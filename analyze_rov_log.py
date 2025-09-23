#!/usr/bin/env python3
"""
Script pour analyser les logs ROV et créer un graphique des positions UTM et heading GPS
"""

import json
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
import numpy as np

def parse_rov_log(file_path):
    """Parse le fichier de log ROV et extrait les données GPS"""
    timestamps = []
    utm_east = []
    utm_north = []
    headings = []
    
    with open(file_path, 'r', encoding='utf-8') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
                
            # Séparer le timestamp du contenu JSON
            parts = line.split(' ', 2)
            if len(parts) < 3 or parts[1] != 'API':
                continue
                
            timestamp_str = parts[0]
            json_str = parts[2]
            
            try:
                # Parser le JSON
                data = json.loads(json_str)
                
                # Vérifier que les données GPS sont valides
                if 'gps' in data and data['gps']['valid'] == 1:
                    # Extraire le timestamp
                    timestamp = datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
                    
                    # Extraire les coordonnées UTM
                    if 'utm' in data['gps']:
                        utm_data = data['gps']['utm']
                        east = utm_data['e']
                        north = utm_data['n']
                        
                        # Extraire le heading
                        heading = data['gps']['hdg']
                        
                        timestamps.append(timestamp)
                        utm_east.append(east)
                        utm_north.append(north)
                        headings.append(heading)
                        
            except (json.JSONDecodeError, KeyError, ValueError) as e:
                continue
    
    return timestamps, utm_east, utm_north, headings

def create_plots(timestamps, utm_east, utm_north, headings):
    """Créer les graphiques des positions UTM et du heading"""
    
    # Convertir en numpy arrays pour faciliter les calculs
    utm_east = np.array(utm_east)
    utm_north = np.array(utm_north)
    headings = np.array(headings)
    
    # Créer une figure avec 3 subplots
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 10))
    fig.suptitle('Analyse des données ROV - Positions UTM et Heading GPS', fontsize=14, fontweight='bold')
    
    # Subplot 1: Trajectoire UTM (vue de dessus)
    ax1.plot(utm_east, utm_north, 'b-', linewidth=1, alpha=0.7, label='Trajectoire')
    ax1.scatter(utm_east[0], utm_north[0], color='green', s=100, label='Début', zorder=5)
    ax1.scatter(utm_east[-1], utm_north[-1], color='red', s=100, label='Fin', zorder=5)
    
    # Ajouter des flèches pour indiquer la direction
    step = max(1, len(utm_east) // 20)  # Afficher environ 20 flèches
    for i in range(0, len(utm_east)-1, step):
        dx = utm_east[i+1] - utm_east[i]
        dy = utm_north[i+1] - utm_north[i]
        if abs(dx) > 0.001 or abs(dy) > 0.001:  # Éviter les flèches trop petites
            ax1.arrow(utm_east[i], utm_north[i], dx, dy, 
                     head_width=0.05, head_length=0.05, fc='red', ec='red', alpha=0.6)
    
    ax1.set_xlabel('UTM East (m)')
    ax1.set_ylabel('UTM North (m)')
    ax1.set_title('Trajectoire UTM du ROV')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.axis('equal')
    
    # Subplot 2: Evolution des coordonnées UTM dans le temps
    ax2.plot(timestamps, utm_east, 'b-', label='UTM East', linewidth=1)
    ax2_twin = ax2.twinx()
    ax2_twin.plot(timestamps, utm_north, 'r-', label='UTM North', linewidth=1)
    
    ax2.set_xlabel('Temps')
    ax2.set_ylabel('UTM East (m)', color='b')
    ax2_twin.set_ylabel('UTM North (m)', color='r')
    ax2.set_title('Evolution des coordonnées UTM dans le temps')
    ax2.grid(True, alpha=0.3)
    ax2.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    ax2.xaxis.set_major_locator(mdates.MinuteLocator(interval=1))
    plt.setp(ax2.xaxis.get_majorticklabels(), rotation=45)
    
    # Ajouter les légendes
    lines1, labels1 = ax2.get_legend_handles_labels()
    lines2, labels2 = ax2_twin.get_legend_handles_labels()
    ax2.legend(lines1 + lines2, labels1 + labels2, loc='upper left')
    
    # Subplot 3: Evolution du heading dans le temps
    ax3.plot(timestamps, headings, 'g-', linewidth=1, label='Heading GPS')
    ax3.set_xlabel('Temps')
    ax3.set_ylabel('Heading (degrés)')
    ax3.set_title('Evolution du Heading GPS dans le temps')
    ax3.grid(True, alpha=0.3)
    ax3.set_ylim(0, 360)
    ax3.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    ax3.xaxis.set_major_locator(mdates.MinuteLocator(interval=1))
    plt.setp(ax3.xaxis.get_majorticklabels(), rotation=45)
    ax3.legend()
    
    plt.tight_layout()
    return fig

def main():
    """Fonction principale"""
    log_file = r'logs\rov.000.log'
    
    print("Parsing du fichier de log...")
    timestamps, utm_east, utm_north, headings = parse_rov_log(log_file)
    
    if not timestamps:
        print("Aucune donnée GPS valide trouvée dans le fichier de log.")
        return
    
    print(f"Trouvé {len(timestamps)} points GPS valides")
    print(f"Période: {timestamps[0]} à {timestamps[-1]}")
    print(f"Zone UTM: {utm_east[0]:.2f}-{utm_east[-1]:.2f} E, {utm_north[0]:.2f}-{utm_north[-1]:.2f} N")
    print(f"Heading: {headings[0]:.1f}° à {headings[-1]:.1f}°")
    
    print("Création des graphiques...")
    fig = create_plots(timestamps, utm_east, utm_north, headings)
    
    # Sauvegarder le graphique
    output_file = 'rov_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Graphique sauvegardé sous: {output_file}")
    
    # Afficher le graphique
    plt.show()

if __name__ == "__main__":
    main()
