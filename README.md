# mod-class-balance

Módulo para AzerothCore (WotLK 3.3.5a) que permite ajustar los multiplicadores
de combate de cada clase de jugador de forma independiente, en caliente, sin
necesidad de reiniciar el servidor.

## Características

- **4 multiplicadores por clase**: daño físico, daño de hechizo, curación y defensa
- **Persistencia en BD**: los cambios se guardan en `acore_world` y sobreviven reinicios
- **Comandos de GM en caliente**: ajusta valores sin recompilar ni reiniciar
- **PvP / PvE configurable**: puedes aplicar los multiplicadores solo en PvE, solo en PvP o en ambos

## Instalación

1. Ejecutar el SQL en la base de datos world:
   ```bash
   mysql -uroot -ppassword acore_world < sql/mod_class_balance.sql
   ```

2. Copiar el fichero de configuración:
   ```bash
   cp conf/mod_class_balance.conf.dist env/dist/etc/modules/mod_class_balance.conf
   ```

3. Recompilar el servidor (el módulo se detecta automáticamente por CMake).

## Comandos in-game

| Comando | Acceso | Descripción |
|---|---|---|
| `.classbalance list` | GM | Muestra todos los multiplicadores |
| `.classbalance info <clase>` | GM | Detalle de una clase con colores |
| `.classbalance set <clase> <tipo> <valor>` | Admin | Ajusta un mult. (live + BD) |
| `.classbalance reset <clase>` | Admin | Resetea clase a 1.0x |
| `.classbalance reload` | Admin | Recarga todos los valores desde la BD |

### Tipos válidos

| Tipo | Alias | Qué modifica |
|---|---|---|
| `phys` | `fisico`, `melee` | Daño físico melee/ranged dado |
| `spell` | `hechizo`, `magia` | Daño de hechizo y DoTs dado |
| `heal` | `cura`, `curacion` | Curación hecha |
| `def` | `defense`, `defensa` | Daño recibido (0.85 = 15% menos daño) |

### Clases válidas (inglés o español)

`warrior`/`guerrero`, `paladin`, `hunter`/`cazador`, `rogue`/`picaro`,
`priest`/`sacerdote`, `deathknight`/`dk`, `shaman`/`chaman`,
`mage`/`mago`, `warlock`/`brujo`, `druid`/`druida`

## Ejemplos de uso

```
# Guerreros hacen 15% más daño físico
.classbalance set warrior phys 1.15

# Magos hacen 10% menos daño de hechizo (nerf)
.classbalance set mage spell 0.90

# Paladines curan 10% más
.classbalance set paladin heal 1.10

# Sacerdotes reciben 15% menos daño (tanque de emergencia)
.classbalance set priest def 0.85

# Ver el estado de todos los Chamanes
.classbalance info shaman

# Resetear todo el Guerrero a valores base
.classbalance reset warrior

# Aplicar cambios editados manualmente en la BD
.classbalance reload
```

## Lógica de aplicación

| Escenario | Físico | Hechizo | Defensa |
|---|---|---|---|
| Player → NPC | `physDmg[clase atacante]` | `spellDmg[clase atacante]` | — |
| NPC → Player | — | — | `defense[clase víctima]` |
| Player → Player (PvP) | `physDmg[atk]` × `defense[vic]` | `spellDmg[atk]` × `defense[vic]` | ambos |

## Tabla de base de datos

```sql
mod_class_balance (
    class_id  TINYINT  PK,  -- 1=Guerrero … 11=Druida
    phys_dmg  FLOAT,        -- mult. daño físico dado
    spell_dmg FLOAT,        -- mult. daño hechizo dado
    healing   FLOAT,        -- mult. curación hecha
    defense   FLOAT         -- mult. daño recibido
)
```

