#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

using namespace std;

// Función para escapar caracteres especiales de Gnuplot (como el _)
string escaparGnuplot(const string& str) {
    string resultado = "";
    for (char c : str) {
        if (c == '_') {
            resultado += "\\_"; // Escapa el guion bajo para Gnuplot
        } else {
            resultado += c;
        }
    }
    return resultado;
}

int main(int argc, char* argv[]) {
    // Ahora exigimos 2 argumentos: el CSV y la ruta de salida del PNG
    if (argc < 3) {
        cout << "==================================================" << endl;
        cout << "  [ERROR] Faltan argumentos" << endl;
        cout << "  Uso correcto: ./generar_grafica <archivo.csv> <ruta_salida.png>" << endl;
        cout << "==================================================" << endl;
        return 1;
    }

    string filename = argv[1];
    string output_image = argv[2];

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "[ERROR] No se pudo abrir el archivo CSV: " << filename << endl;
        return 1;
    }

    string header_line;
    if (!getline(file, header_line)) {
        file.close();
        cerr << "[ERROR] El archivo CSV está vacío." << endl;
        return 1;
    }
    file.close();

    vector<string> headers;
    stringstream ss(header_line);
    string col;
    while (getline(ss, col, ',')) {
        if (!col.empty() && col.back() == '\r') col.pop_back();
        headers.push_back(col);
    }

    if (headers.size() < 2) {
        cerr << "[ERROR] El CSV debe tener al menos dos columnas." << endl;
        return 1;
    }

    // Crear un script temporal para Gnuplot de forma dinámica
    ofstream plt("script_temp.plt");
    plt << "set terminal png enhanced font 'Verdana,10' size 800,600\n";
    plt << "set output '" << output_image << "'\n"; // Usamos la ruta pasada por parámetro
    plt << "set title 'Comparativa de Rendimiento'\n";
    plt << "set xlabel '" << escaparGnuplot(headers[0]) << "'\n";
    plt << "set ylabel 'Tiempo de Ejecucion (segundos)'\n";
    plt << "set grid\n";
    plt << "set datafile separator ','\n";
    
    plt << "plot ";
    for (size_t i = 1; i < headers.size(); ++i) {
        int point_type = 7 + (i - 1) * 2;
        string titulo_limpio = escaparGnuplot(headers[i]);
        
        plt << "'" << filename << "' using 1:" << (i + 1) 
            << " with linespoints pt " << point_type << " ps 1.2 lw 2 title '" << titulo_limpio << "'";
        
        if (i < headers.size() - 1) {
            plt << ", \\\n     ";
        }
    }
    plt << "\n";
    plt.close();

    // Ejecutar gnuplot con el script generado
    int ret = system("gnuplot script_temp.plt");
    if (ret == 0) {
        cout << "[¡ÉXITO!] Gráfica guardada en: " << output_image << endl;
        (void)system("rm -f script_temp.plt");
    } else {
        cerr << "[ERROR] No se pudo ejecutar 'gnuplot'." << endl;
    }

    return 0;
}