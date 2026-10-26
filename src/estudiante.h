// =============================================================================
//  estudiante.h
//  -----------------------------------------------------------------------------
//  MODELO de datos de la aplicación.
//
//  Un "modelo" es simplemente una estructura que representa UNA fila de una
//  tabla de la base de datos. Aquí cada Estudiante corresponde a una fila de
//  la tabla `estudiantes`.
//
//  También definimos cómo convertir un Estudiante a JSON y viceversa, porque
//  el navegador (JavaScript) y el servidor (C++) se comunican usando JSON.
// =============================================================================
#pragma once

#include <string>
#include "json.hpp"   // nlohmann/json: librería de un solo archivo para JSON

using json = nlohmann::json;

// -----------------------------------------------------------------------------
//  Estudiante: una fila de la tabla `estudiantes`
// -----------------------------------------------------------------------------
struct Estudiante {
    int         id = 0;         // Clave primaria (la asigna SQLite automáticamente)
    std::string nombre;         // Nombre del estudiante
    std::string apellido;       // Apellido(s)
    std::string seccion;        // Por ejemplo "11-1", "11-2"...
    double      nota = 0.0;     // Nota de 0 a 100
    std::string creado_en;      // Fecha de creación (la pone SQLite)
};

// -----------------------------------------------------------------------------
//  Estadisticas: resultado de una consulta de agregación (COUNT, AVG, MAX...)
// -----------------------------------------------------------------------------
struct Estadisticas {
    int    total      = 0;
    double promedio   = 0.0;
    double nota_max   = 0.0;
    double nota_min   = 0.0;
    int    aprobados  = 0;      // nota >= 70
    int    reprobados = 0;      // nota <  70
};

// -----------------------------------------------------------------------------
//  Conversión a JSON
//  nlohmann/json busca automáticamente funciones llamadas `to_json` y
//  `from_json` para poder hacer:   json j = estudiante;
// -----------------------------------------------------------------------------
inline void to_json(json& j, const Estudiante& e) {
    j = json{
        {"id",        e.id},
        {"nombre",    e.nombre},
        {"apellido",  e.apellido},
        {"seccion",   e.seccion},
        {"nota",      e.nota},
        {"creado_en", e.creado_en}
    };
}

inline void to_json(json& j, const Estadisticas& s) {
    j = json{
        {"total",      s.total},
        {"promedio",   s.promedio},
        {"nota_max",   s.nota_max},
        {"nota_min",   s.nota_min},
        {"aprobados",  s.aprobados},
        {"reprobados", s.reprobados}
    };
}
