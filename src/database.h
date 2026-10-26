// =============================================================================
//  database.h
//  -----------------------------------------------------------------------------
//  CAPA DE ACCESO A DATOS.
//
//  Esta clase es la ÚNICA parte del programa que "habla" con SQLite.
//  El resto del programa (las rutas HTTP en main.cpp) no escribe SQL: solo
//  llama a métodos como listar(), crear(), eliminar()...
//
//  Ventajas de separar así el código:
//    * Si mañana cambiamos SQLite por MySQL, solo tocamos database.cpp.
//    * Es más fácil de leer, probar y explicar.
// =============================================================================
#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "estudiante.h"

// Declaración adelantada: no necesitamos incluir sqlite3.h en este archivo,
// solo saber que existe un tipo llamado sqlite3.
struct sqlite3;

class Database {
public:
    // Abre (o crea, si no existe) el archivo de base de datos.
    explicit Database(const std::string& ruta_archivo);

    // Cierra la conexión automáticamente al destruirse el objeto (RAII).
    ~Database();

    // Prohibimos copiar el objeto: no queremos dos objetos cerrando la
    // misma conexión.
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // Crea las tablas si no existen e inserta datos de ejemplo la 1ª vez.
    void inicializar();

    // ---------------- Operaciones CRUD ----------------
    // C = Create, R = Read, U = Update, D = Delete

    // R: lista todos los estudiantes. Si `filtro` no está vacío, busca por
    //    nombre, apellido o sección que lo contengan.
    std::vector<Estudiante> listar(const std::string& filtro = "");

    // R: busca un estudiante por id. Si no existe devuelve std::nullopt.
    std::optional<Estudiante> obtener(int id);

    // C: inserta un estudiante y lo devuelve con su id ya asignado.
    Estudiante crear(const Estudiante& e);

    // U: actualiza un estudiante. Devuelve false si el id no existe.
    bool actualizar(int id, const Estudiante& e);

    // D: elimina un estudiante. Devuelve false si el id no existe.
    bool eliminar(int id);

    // Consulta con funciones de agregación (COUNT, AVG, MIN, MAX).
    Estadisticas estadisticas();

private:
    sqlite3*   db_ = nullptr;   // "Conexión" a la base de datos
    std::mutex mutex_;          // El servidor atiende varias peticiones a la
                                // vez (hilos). El mutex evita que dos hilos
                                // usen la conexión al mismo tiempo.

    // Ejecuta SQL que no devuelve filas (CREATE TABLE, etc.)
    void ejecutar(const std::string& sql);
};
