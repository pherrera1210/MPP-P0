#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdlib>

using namespace std;

// Función para dividir un string por comas (CSV)
vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    string token;
    stringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    // Ahora esperamos: ./generador_tablas <archivo.csv> <tiempo_secuencial>
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <archivo.csv> <t_secuencial>\n";
        return 1;
    }

    string ruta_csv = argv[1];
    double t_secuencial = stod(argv[2]);

    ifstream archivo(ruta_csv);
    if (!archivo.is_open()) {
        cerr << "Error al abrir el archivo: " << ruta_csv << "\n";
        return 1;
    }

    string linea;
    // Leer la cabecera del CSV (ej: Hilos,Static,Static_1,Static_4,...)
    if (!getline(archivo, linea)) {
        cerr << "El archivo está vacío.\n";
        return 1;
    }

    vector<string> cabecera = split(linea, ',');
    if (cabecera.empty()) return 1;

    // La primera columna suele ser "Hilos"
    string nombre_col_hilos = cabecera[0];

    // Procesamos cada variante (cada columna a partir de la 1)
    // Para cada columna, tendremos sus valores para cada número de hilos.
    // Primero almacenamos todas las filas en memoria para poder recorrerlas cómodamente.
    vector<vector<string>> filas;
    while (getline(archivo, linea)) {
        if (linea.empty()) continue;
        vector<string> columnas = split(linea, ',');
        if (!columnas.empty()) {
            filas.push_back(columnas);
        }
    }
    archivo.close();

    // Recorremos desde la columna 1 hasta el final (las diferentes políticas/variantes)
    for (size_t col = 1; col < cabecera.size(); ++col) {
        string nombre_variante = cabecera[col];

        cout << "\n=================================================================================\n";
        cout << " TABLA DE RENDIMIENTO: " << nombre_variante << " (T. Secuencial = " << t_secuencial << "s)\n";
        cout << "=================================================================================\n";
        cout << setw(10) << "Nº Hilos" << " | " 
             << setw(14) << "T. Ejecución" << " | " 
             << setw(12) << "Speed-up" << " | " 
             << setw(12) << "Eficiencia" << "\n";
        cout << "---------------------------------------------------------------------------------\n";

        for (const auto& fila : filas) {
            if (fila.size() <= col) continue;

            int hilos = stoi(fila[0]);
            double t_paralelo = stod(fila[col]);

            double speedup = (t_paralelo > 0.0) ? (t_secuencial / t_paralelo) : 0.0;
            double eficiencia = (hilos > 0) ? (speedup / hilos) : 0.0;

            cout << setw(10) << hilos << " | " 
                 << setw(14) << fixed << setprecision(4) << t_paralelo << " | " 
                 << setw(12) << fixed << setprecision(2) << speedup << " | " 
                 << setw(12) << fixed << setprecision(4) << eficiencia << "\n";
        }
        cout << "=================================================================================\n";
    }

    return 0;
}