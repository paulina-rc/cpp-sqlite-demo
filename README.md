# Registro de Estudiantes — Demo Web con backend en C++ y SQLite

Aplicación web completa que corre **100 % en local**:

- **Frontend:** HTML + CSS + JavaScript puro (sin frameworks).
- **Backend:** servidor HTTP escrito en **C++17**.
- **Base de datos:** **SQLite**, guardada en un único archivo (`data/escuela.db`).

Permite crear, listar, buscar, editar y eliminar estudiantes (CRUD), y muestra
estadísticas calculadas con SQL (promedio, aprobados, reprobados).

---

## 1. Inicio rápido (Windows)

Requisito: tener **g++** instalado (por ejemplo con [MSYS2](https://www.msys2.org/),
paquete `mingw-w64-ucrt-x86_64-gcc`) y la carpeta `C:\msys64\ucrt64\bin` en el `PATH`.

1. Doble clic en **`compilar.bat`** (la primera vez tarda ~1 min porque compila SQLite).
2. Doble clic en **`iniciar.bat`** → se abre el navegador en <http://localhost:8080>.
3. Para detener el servidor: `Ctrl + C` en la ventana negra.

Alternativa con `make` (MSYS2, Linux o macOS):

```bash
mingw32-make        # compila   (en Linux/macOS: make)
mingw32-make run    # compila y ejecuta
mingw32-make clean  # borra la carpeta build/
```

> **Importante:** el servidor debe ejecutarse **desde la carpeta raíz del
> proyecto**, porque busca las carpetas `public/` y `data/` de forma relativa.
> Puerto distinto: `build\servidor.exe 3000`.

---

## 2. ¿Cómo funciona? (arquitectura)

```
┌──────────────────────┐   HTTP + JSON    ┌──────────────────────┐    SQL    ┌──────────────┐
│  NAVEGADOR           │ ───────────────► │  SERVIDOR C++        │ ────────► │  SQLite      │
│  index.html          │   fetch('/api')  │  main.cpp   (rutas)  │           │  data/       │
│  app.js   styles.css │ ◄─────────────── │  database.cpp (SQL)  │ ◄──────── │  escuela.db  │
└──────────────────────┘                  └──────────────────────┘   filas   └──────────────┘
```

1. El navegador pide `http://localhost:8080/` y el servidor le entrega los
   archivos de `public/` (HTML, CSS, JS).
2. `app.js` usa `fetch()` para llamar a la **API REST** (`/api/...`).
3. `main.cpp` recibe la petición, **valida** los datos y llama a la clase `Database`.
4. `Database` ejecuta la consulta SQL en SQLite y devuelve objetos `Estudiante`.
5. `main.cpp` convierte el resultado a **JSON** y lo responde al navegador.
6. `app.js` dibuja la tabla con los datos recibidos.

El navegador **nunca** toca la base de datos directamente: siempre pasa por el
servidor, que es quien decide qué está permitido.

---

## 3. Estructura de carpetas

```
demo1/
├── src/                     ← BACKEND (C++)
│   ├── main.cpp             Servidor HTTP: rutas de la API y validación
│   ├── database.h / .cpp    Clase Database: TODO el SQL está aquí
│   └── estudiante.h         Modelo de datos + conversión a JSON
│
├── public/                  ← FRONTEND (lo que ve el usuario)
│   ├── index.html           Estructura de la página
│   ├── styles.css           Apariencia
│   └── app.js               Lógica: llamadas a la API y dibujo de la tabla
│
├── third_party/             ← Librerías externas (no se modifican)
│   ├── httplib/httplib.h    cpp-httplib 0.18.3  – servidor HTTP
│   ├── json/json.hpp        nlohmann/json 3.11.3 – manejo de JSON
│   └── sqlite/sqlite3.c/.h  SQLite 3.49.1 (amalgamation) – base de datos
│
├── data/escuela.db          Base de datos (se crea sola al primer arranque)
├── build/                   Archivos compilados (se crea al compilar)
├── compilar.bat             Compila en Windows sin make
├── iniciar.bat              Arranca el servidor y abre el navegador
└── Makefile                 Compilación con make
```

Las tres librerías son de **un solo archivo** y se incluyen en el proyecto, así
que no hay que instalar nada aparte de g++.

---

## 4. La base de datos

Tabla única `estudiantes` (se crea en `Database::inicializar()`):

| Columna     | Tipo    | Restricciones                               |
|-------------|---------|---------------------------------------------|
| `id`        | INTEGER | PRIMARY KEY AUTOINCREMENT                   |
| `nombre`    | TEXT    | NOT NULL                                    |
| `apellido`  | TEXT    | NOT NULL                                    |
| `seccion`   | TEXT    | NOT NULL                                    |
| `nota`      | REAL    | NOT NULL, `CHECK (nota BETWEEN 0 AND 100)`  |
| `creado_en` | TEXT    | DEFAULT fecha/hora actual                   |

La primera vez se insertan 6 estudiantes de ejemplo. Para **reiniciar** la base
de datos, detenga el servidor y borre `data/escuela.db`.

Se puede abrir el archivo con cualquier visor de SQLite, por ejemplo
[DB Browser for SQLite](https://sqlitebrowser.org/), para ver los datos o
ejecutar consultas a mano.

### Consultas SQL utilizadas

| Operación     | SQL                                                                  |
|---------------|----------------------------------------------------------------------|
| Listar        | `SELECT ... FROM estudiantes ORDER BY apellido, nombre`              |
| Buscar        | `... WHERE nombre LIKE ?1 OR apellido LIKE ?1 OR seccion LIKE ?1`    |
| Obtener uno   | `SELECT ... WHERE id = ?`                                            |
| Crear         | `INSERT INTO estudiantes (nombre, apellido, seccion, nota) VALUES (?, ?, ?, ?)` |
| Actualizar    | `UPDATE estudiantes SET nombre = ?, ... WHERE id = ?`                |
| Eliminar      | `DELETE FROM estudiantes WHERE id = ?`                               |
| Estadísticas  | `SELECT COUNT(*), AVG(nota), MAX(nota), MIN(nota), SUM(CASE ...)`    |

Los `?` son **parámetros**: el valor se envía por separado con
`sqlite3_bind_*()`, nunca pegado al texto del SQL. Esto evita la
**inyección SQL** (si alguien busca `' OR 1=1 --`, simplemente no encuentra nada).

---

## 5. API REST

Todas las respuestas son JSON. Los errores tienen la forma `{"error": "mensaje"}`.

| Método   | Ruta                       | Cuerpo (JSON)         | Respuesta                    |
|----------|----------------------------|-----------------------|------------------------------|
| `GET`    | `/api/estudiantes`         | —                     | `200` arreglo de estudiantes |
| `GET`    | `/api/estudiantes?q=ana`   | —                     | `200` arreglo filtrado       |
| `GET`    | `/api/estudiantes/{id}`    | —                     | `200` estudiante / `404`     |
| `POST`   | `/api/estudiantes`         | estudiante sin `id`   | `201` estudiante creado / `400` |
| `PUT`    | `/api/estudiantes/{id}`    | estudiante sin `id`   | `200` actualizado / `400` / `404` |
| `DELETE` | `/api/estudiantes/{id}`    | —                     | `200 {"ok": true}` / `404`   |
| `GET`    | `/api/estadisticas`        | —                     | `200` objeto de estadísticas |

Ejemplo de cuerpo para `POST` / `PUT`:

```json
{ "nombre": "Ana", "apellido": "Rodríguez Mora", "seccion": "11-1", "nota": 95 }
```

Ejemplo de respuesta de `/api/estadisticas`:

```json
{ "total": 6, "promedio": 77.5, "nota_max": 95.0, "nota_min": 59.0, "aprobados": 4, "reprobados": 2 }
```

Se puede probar la API sin el navegador, con `curl`:

```bash
curl http://localhost:8080/api/estudiantes
curl -X DELETE http://localhost:8080/api/estudiantes/3
```

### Códigos de estado HTTP usados

| Código | Significado                                   |
|--------|-----------------------------------------------|
| 200    | OK                                            |
| 201    | Creado (respuesta a un POST exitoso)          |
| 400    | Datos inválidos (falta un campo, nota fuera de rango, JSON mal formado) |
| 404    | No existe un estudiante con ese id            |
| 500    | Error interno (por ejemplo, error de SQLite)  |

---

## 6. Conceptos que ilustra el proyecto

| Concepto                       | Dónde verlo                                          |
|--------------------------------|------------------------------------------------------|
| Arquitectura cliente-servidor  | `public/app.js` ↔ `src/main.cpp`                     |
| API REST y verbos HTTP         | rutas en `src/main.cpp`                              |
| Separación en capas            | rutas (`main.cpp`) / datos (`database.cpp`) / modelo (`estudiante.h`) |
| CRUD con SQL                   | `src/database.cpp`                                   |
| Consultas preparadas (seguridad) | clase `Sentencia` en `src/database.cpp`            |
| RAII (manejo de recursos en C++) | `Database` y `Sentencia` cierran solos su recurso  |
| Concurrencia (mutex)           | `std::mutex` en `Database`                           |
| Validación en servidor         | `validarYConvertir()` en `src/main.cpp`              |
| Serialización JSON             | `to_json()` en `src/estudiante.h`                    |
| Prevención de XSS              | uso de `textContent` en `public/app.js`              |
| Programación asíncrona (JS)    | `async/await` + `fetch()` en `public/app.js`         |

---

## 7. Ideas para extender (ejercicios)

1. Agregar un campo `correo` a la tabla (SQL, struct, validación, formulario y tabla).
2. Crear una segunda tabla `secciones` y relacionarla con `FOREIGN KEY`.
3. Ordenar la tabla al hacer clic en el encabezado de una columna (`ORDER BY` dinámico **seguro**: lista blanca de columnas).
4. Paginación: `LIMIT ? OFFSET ?` y botones *Anterior / Siguiente*.
5. Nueva ruta `GET /api/estadisticas/secciones` con `GROUP BY seccion`.
6. Exportar la lista a CSV desde una ruta del servidor.

---

## 8. Solución de problemas

| Problema                                  | Solución                                                     |
|-------------------------------------------|--------------------------------------------------------------|
| `No se encontro g++ en el PATH`           | Instalar MSYS2 y agregar `C:\msys64\ucrt64\bin` al PATH.     |
| `No se encontro public/index.html`        | Ejecutar el servidor desde la carpeta raíz (usar `iniciar.bat`). |
| `No se pudo abrir el puerto 8080`         | Otro programa usa ese puerto: `build\servidor.exe 3000`.     |
| El firewall de Windows pregunta           | El servidor solo escucha en `127.0.0.1` (local); se puede cancelar. |
| La página muestra "No se pudo conectar"   | El servidor no está corriendo: revisar la ventana negra.     |
| Quiero empezar con la BD limpia           | Detener el servidor y borrar `data/escuela.db`.              |
