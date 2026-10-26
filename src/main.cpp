// =============================================================================
//  main.cpp
//  -----------------------------------------------------------------------------
//  PUNTO DE ENTRADA DEL SERVIDOR.
//
//  Aquí se:
//    1. Abre la base de datos SQLite (archivo data/escuela.db).
//    2. Crea un servidor HTTP con cpp-httplib.
//    3. Sirve los archivos de la carpeta public/ (HTML, CSS, JS).
//    4. Define la API REST en /api/... que el JavaScript del navegador usa
//       para leer y modificar datos.
//
//  API REST (todas las respuestas son JSON):
//
//     Método  Ruta                     Acción
//     ------  -----------------------  -------------------------------------
//     GET     /api/estudiantes         Listar (opcional ?q=texto para buscar)
//     GET     /api/estudiantes/{id}    Obtener uno
//     POST    /api/estudiantes         Crear     (cuerpo JSON)
//     PUT     /api/estudiantes/{id}    Actualizar(cuerpo JSON)
//     DELETE  /api/estudiantes/{id}    Eliminar
//     GET     /api/estadisticas        Totales, promedio, aprobados...
//
//  Uso:   servidor.exe [puerto]        (por defecto 8080)
// =============================================================================
#include <filesystem>
#include <iostream>
#include <string>

#include "httplib.h"     // cpp-httplib: servidor HTTP en un solo archivo
#include "database.h"

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
//  Funciones auxiliares para responder
// -----------------------------------------------------------------------------

// Envía un objeto JSON con el código de estado indicado (200, 201, 404...).
static void responderJson(httplib::Response& res, const json& cuerpo, int estado = 200) {
    res.status = estado;
    // dump(2) = JSON con sangría de 2 espacios (más fácil de leer al depurar).
    // ensure_ascii=false para que las tildes se envíen tal cual en UTF-8.
    res.set_content(cuerpo.dump(2, ' ', false), "application/json; charset=utf-8");
}

static void responderError(httplib::Response& res, int estado, const std::string& mensaje) {
    responderJson(res, json{{"error", mensaje}}, estado);
}

// -----------------------------------------------------------------------------
//  Validación de los datos que envía el cliente.
//  Nunca hay que confiar en lo que llega del navegador: siempre se valida
//  en el servidor, aunque el formulario HTML también valide.
//
//  Devuelve un mensaje de error, o una cadena vacía si todo está bien.
// -----------------------------------------------------------------------------
static std::string validarYConvertir(const std::string& cuerpo, Estudiante& e) {
    // parse(texto, nullptr, false) no lanza excepción: devuelve "discarded"
    // si el JSON está mal formado.
    json j = json::parse(cuerpo, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return "El cuerpo debe ser un objeto JSON válido";

    if (!j.contains("nombre")   || !j["nombre"].is_string())   return "Falta 'nombre' (texto)";
    if (!j.contains("apellido") || !j["apellido"].is_string()) return "Falta 'apellido' (texto)";
    if (!j.contains("seccion")  || !j["seccion"].is_string())  return "Falta 'seccion' (texto)";
    if (!j.contains("nota")     || !j["nota"].is_number())     return "Falta 'nota' (número)";

    e.nombre   = j["nombre"].get<std::string>();
    e.apellido = j["apellido"].get<std::string>();
    e.seccion  = j["seccion"].get<std::string>();
    e.nota     = j["nota"].get<double>();

    if (e.nombre.empty())   return "El nombre no puede estar vacío";
    if (e.apellido.empty()) return "El apellido no puede estar vacío";
    if (e.seccion.empty())  return "La sección no puede estar vacía";
    if (e.nota < 0 || e.nota > 100) return "La nota debe estar entre 0 y 100";

    return "";
}

// =============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);   // Para que la consola de Windows muestre tildes (UTF-8)
#endif
    int puerto = (argc > 1) ? std::atoi(argv[1]) : 8080;

    // Las rutas son relativas a la carpeta desde donde se ejecuta el programa.
    // Por eso hay que ejecutarlo desde la raíz del proyecto (ver README).
    const fs::path carpeta_publica = "public";
    const fs::path carpeta_datos   = "data";

    if (!fs::exists(carpeta_publica / "index.html")) {
        std::cerr << "No se encontro public/index.html.\n"
                     "Ejecute el servidor desde la carpeta raiz del proyecto.\n";
        return 1;
    }
    fs::create_directories(carpeta_datos);

    // ---------------------------------------------------------------------
    // 1. Base de datos
    // ---------------------------------------------------------------------
    Database db((carpeta_datos / "escuela.db").string());
    db.inicializar();

    // ---------------------------------------------------------------------
    // 2. Servidor HTTP
    // ---------------------------------------------------------------------
    httplib::Server svr;

    // Archivos estáticos: http://localhost:8080/  ->  public/index.html
    svr.set_mount_point("/", carpeta_publica.string());

    // Registro en consola de cada petición (útil para ver qué está pasando).
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << req.method << " " << req.path << " -> " << res.status << "\n";
    });

    // Si un manejador lanza una excepción (p. ej. error de SQLite), se
    // responde 500 en lugar de cerrar el programa.
    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        try { std::rethrow_exception(ep); }
        catch (const std::exception& ex) { responderError(res, 500, ex.what()); }
        catch (...)                      { responderError(res, 500, "Error desconocido"); }
    });

    // ---------------------------------------------------------------------
    // 3. Rutas de la API
    //    Las rutas con (\d+) son expresiones regulares: capturan el id
    //    numérico, que luego se lee con req.matches[1].
    // ---------------------------------------------------------------------

    // GET /api/estudiantes?q=texto
    svr.Get("/api/estudiantes", [&](const httplib::Request& req, httplib::Response& res) {
        std::string filtro = req.has_param("q") ? req.get_param_value("q") : "";
        responderJson(res, db.listar(filtro));   // vector<Estudiante> -> arreglo JSON
    });

    // GET /api/estudiantes/5
    svr.Get(R"(/api/estudiantes/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto e = db.obtener(id);
        if (!e) return responderError(res, 404, "Estudiante no encontrado");
        responderJson(res, *e);
    });

    // POST /api/estudiantes   { "nombre": "...", "apellido": "...", ... }
    svr.Post("/api/estudiantes", [&](const httplib::Request& req, httplib::Response& res) {
        Estudiante e;
        std::string error = validarYConvertir(req.body, e);
        if (!error.empty()) return responderError(res, 400, error);

        responderJson(res, db.crear(e), 201);    // 201 = Created
    });

    // PUT /api/estudiantes/5
    svr.Put(R"(/api/estudiantes/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        Estudiante e;
        std::string error = validarYConvertir(req.body, e);
        if (!error.empty()) return responderError(res, 400, error);

        if (!db.actualizar(id, e)) return responderError(res, 404, "Estudiante no encontrado");
        responderJson(res, *db.obtener(id));
    });

    // DELETE /api/estudiantes/5
    svr.Delete(R"(/api/estudiantes/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        if (!db.eliminar(id)) return responderError(res, 404, "Estudiante no encontrado");
        responderJson(res, json{{"ok", true}});
    });

    // GET /api/estadisticas
    svr.Get("/api/estadisticas", [&](const httplib::Request&, httplib::Response& res) {
        responderJson(res, db.estadisticas());
    });

    // ---------------------------------------------------------------------
    // 4. Arrancar
    // ---------------------------------------------------------------------
    std::cout << "==============================================\n"
              << "  Servidor listo en  http://localhost:" << puerto << "\n"
              << "  Base de datos:     data/escuela.db\n"
              << "  Ctrl+C para detener\n"
              << "==============================================\n";

    // listen() bloquea aquí hasta que se cierre el programa.
    if (!svr.listen("127.0.0.1", puerto)) {
        std::cerr << "No se pudo abrir el puerto " << puerto << " (¿ya está en uso?)\n";
        return 1;
    }
    return 0;
}
