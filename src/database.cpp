// =============================================================================
//  database.cpp
//  -----------------------------------------------------------------------------
//  Implementación de la clase Database usando la API en C de SQLite.
//
//  Flujo típico para ejecutar una consulta en SQLite:
//
//     1. sqlite3_prepare_v2()  -> "compila" el texto SQL en una sentencia
//     2. sqlite3_bind_*()      -> rellena los parámetros  ?  con valores
//     3. sqlite3_step()        -> ejecuta; devuelve SQLITE_ROW por cada fila
//                                 y SQLITE_DONE cuando termina
//     4. sqlite3_column_*()    -> lee las columnas de la fila actual
//     5. sqlite3_finalize()    -> libera la sentencia
//
//  IMPORTANTE (seguridad): nunca armamos el SQL concatenando texto que viene
//  del usuario. Usamos parámetros  ?  + sqlite3_bind_*. Así evitamos los
//  ataques de "inyección SQL".
// =============================================================================
#include "database.h"

#include <stdexcept>

#include "sqlite3.h"

// -----------------------------------------------------------------------------
//  Sentencia: pequeño envoltorio RAII sobre sqlite3_stmt.
//
//  RAII = "Resource Acquisition Is Initialization": el constructor adquiere
//  el recurso y el destructor lo libera. Así nunca olvidamos llamar a
//  sqlite3_finalize(), incluso si ocurre una excepción.
// -----------------------------------------------------------------------------
namespace {

class Sentencia {
public:
    Sentencia(sqlite3* db, const std::string& sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("Error preparando SQL: ") + sqlite3_errmsg(db));
        }
    }
    ~Sentencia() { sqlite3_finalize(stmt_); }

    Sentencia(const Sentencia&)            = delete;
    Sentencia& operator=(const Sentencia&) = delete;

    // Los parámetros en SQLite se numeran desde 1.
    void bind(int i, const std::string& v) { sqlite3_bind_text(stmt_, i, v.c_str(), -1, SQLITE_TRANSIENT); }
    void bind(int i, double v)             { sqlite3_bind_double(stmt_, i, v); }
    void bind(int i, int v)                { sqlite3_bind_int(stmt_, i, v); }

    // Avanza a la siguiente fila. true = hay fila; false = terminó.
    bool siguiente() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW)  return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(std::string("Error ejecutando SQL: ") + sqlite3_errmsg(db_));
    }

    // Lectura de columnas (se numeran desde 0).
    int         entero(int col) { return sqlite3_column_int(stmt_, col); }
    double      real(int col)   { return sqlite3_column_double(stmt_, col); }
    std::string texto(int col) {
        auto p = sqlite3_column_text(stmt_, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    }

private:
    sqlite3*      db_   = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

// Convierte la fila actual de un SELECT en un Estudiante.
// El orden de columnas debe coincidir con COLUMNAS (ver abajo).
Estudiante leerEstudiante(Sentencia& s) {
    Estudiante e;
    e.id        = s.entero(0);
    e.nombre    = s.texto(1);
    e.apellido  = s.texto(2);
    e.seccion   = s.texto(3);
    e.nota      = s.real(4);
    e.creado_en = s.texto(5);
    return e;
}

const std::string COLUMNAS = "id, nombre, apellido, seccion, nota, creado_en";

} // namespace

// =============================================================================
//  Constructor / destructor
// =============================================================================
Database::Database(const std::string& ruta_archivo) {
    // sqlite3_open crea el archivo si no existe.
    if (sqlite3_open(ruta_archivo.c_str(), &db_) != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("No se pudo abrir la base de datos: " + msg);
    }
}

Database::~Database() {
    sqlite3_close(db_);
}

void Database::ejecutar(const std::string& sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string msg = error ? error : "desconocido";
        sqlite3_free(error);
        throw std::runtime_error("Error SQL: " + msg);
    }
}

// =============================================================================
//  Esquema de la base de datos
// =============================================================================
void Database::inicializar() {
    std::lock_guard<std::mutex> lock(mutex_);

    // CHECK obliga a que la nota esté entre 0 y 100 a nivel de base de datos.
    ejecutar(R"SQL(
        CREATE TABLE IF NOT EXISTS estudiantes (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            nombre     TEXT    NOT NULL,
            apellido   TEXT    NOT NULL,
            seccion    TEXT    NOT NULL,
            nota       REAL    NOT NULL CHECK (nota BETWEEN 0 AND 100),
            creado_en  TEXT    NOT NULL DEFAULT (datetime('now', 'localtime'))
        );
    )SQL");

    // Si la tabla está vacía, insertamos algunos datos de ejemplo.
    Sentencia contar(db_, "SELECT COUNT(*) FROM estudiantes");
    contar.siguiente();
    if (contar.entero(0) == 0) {
        ejecutar(R"SQL(
            INSERT INTO estudiantes (nombre, apellido, seccion, nota) VALUES
              ('Ana',    'Rodríguez Mora',   '11-1', 95),
              ('Carlos', 'Jiménez Solís',    '11-1', 68),
              ('María',  'Vargas Quesada',   '11-2', 88),
              ('José',   'Hernández Rojas',  '11-2', 74),
              ('Sofía',  'Castro Alfaro',    '11-3', 59),
              ('Daniel', 'Araya Chaves',     '11-3', 81);
        )SQL");
    }
}

// =============================================================================
//  CRUD
// =============================================================================
std::vector<Estudiante> Database::listar(const std::string& filtro) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Estudiante> resultado;

    if (filtro.empty()) {
        Sentencia s(db_, "SELECT " + COLUMNAS + " FROM estudiantes ORDER BY apellido, nombre");
        while (s.siguiente()) resultado.push_back(leerEstudiante(s));
    } else {
        // LIKE '%texto%' busca el texto en cualquier posición.
        Sentencia s(db_, "SELECT " + COLUMNAS + " FROM estudiantes "
                         "WHERE nombre LIKE ?1 OR apellido LIKE ?1 OR seccion LIKE ?1 "
                         "ORDER BY apellido, nombre");
        s.bind(1, "%" + filtro + "%");
        while (s.siguiente()) resultado.push_back(leerEstudiante(s));
    }
    return resultado;
}

std::optional<Estudiante> Database::obtener(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "SELECT " + COLUMNAS + " FROM estudiantes WHERE id = ?");
    s.bind(1, id);
    if (s.siguiente()) return leerEstudiante(s);
    return std::nullopt;
}

Estudiante Database::crear(const Estudiante& e) {
    int nuevo_id;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Sentencia s(db_, "INSERT INTO estudiantes (nombre, apellido, seccion, nota) VALUES (?, ?, ?, ?)");
        s.bind(1, e.nombre);
        s.bind(2, e.apellido);
        s.bind(3, e.seccion);
        s.bind(4, e.nota);
        s.siguiente();

        // Id que SQLite asignó a la fila recién insertada.
        nuevo_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    }
    // Volvemos a leerlo para obtener también la fecha `creado_en`.
    // (Se hace fuera del bloque anterior porque obtener() bloquea el mutex.)
    return *obtener(nuevo_id);
}

bool Database::actualizar(int id, const Estudiante& e) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "UPDATE estudiantes SET nombre = ?, apellido = ?, seccion = ?, nota = ? WHERE id = ?");
    s.bind(1, e.nombre);
    s.bind(2, e.apellido);
    s.bind(3, e.seccion);
    s.bind(4, e.nota);
    s.bind(5, id);
    s.siguiente();

    // sqlite3_changes: cuántas filas modificó la última sentencia.
    return sqlite3_changes(db_) > 0;
}

bool Database::eliminar(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Sentencia s(db_, "DELETE FROM estudiantes WHERE id = ?");
    s.bind(1, id);
    s.siguiente();
    return sqlite3_changes(db_) > 0;
}

// =============================================================================
//  Consultas de agregación
// =============================================================================
Estadisticas Database::estadisticas() {
    std::lock_guard<std::mutex> lock(mutex_);

    // COALESCE(x, 0) devuelve 0 cuando x es NULL (tabla vacía).
    Sentencia s(db_, R"SQL(
        SELECT COUNT(*),
               COALESCE(AVG(nota), 0),
               COALESCE(MAX(nota), 0),
               COALESCE(MIN(nota), 0),
               COALESCE(SUM(CASE WHEN nota >= 70 THEN 1 ELSE 0 END), 0),
               COALESCE(SUM(CASE WHEN nota <  70 THEN 1 ELSE 0 END), 0)
        FROM estudiantes
    )SQL");
    s.siguiente();

    Estadisticas r;
    r.total      = s.entero(0);
    r.promedio   = s.real(1);
    r.nota_max   = s.real(2);
    r.nota_min   = s.real(3);
    r.aprobados  = s.entero(4);
    r.reprobados = s.entero(5);
    return r;
}
