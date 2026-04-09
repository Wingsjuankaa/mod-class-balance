-- ============================================================
-- mod-class-balance: Ajuste de multiplicadores por clase
-- Base de datos: acore_world (WorldDatabase)
-- ============================================================
-- Idempotente: usa CREATE TABLE IF NOT EXISTS + INSERT IGNORE
-- ============================================================

CREATE TABLE IF NOT EXISTS `mod_class_balance` (
    `class_id`  TINYINT UNSIGNED NOT NULL
        COMMENT '1=Guerrero 2=Paladín 3=Cazador 4=Pícaro 5=Sacerdote 6=Caballero de la Muerte 7=Chamán 8=Mago 9=Brujo 11=Druida',
    `phys_dmg`  FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. daño físico dado (melee/ranged). 1.2 = +20%',
    `spell_dmg` FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. daño de hechizo dado (spells, DoTs). 0.9 = -10%',
    `healing`   FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. curación hecha. 1.1 = +10%',
    `defense`   FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. daño recibido (todas las fuentes). 0.85 = 15% menos daño',
    PRIMARY KEY (`class_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Multiplicadores de balance por clase de jugador (mod-class-balance)';

-- Valores iniciales: todas las clases en 1.0 (sin modificación)
INSERT IGNORE INTO `mod_class_balance`
    (`class_id`, `phys_dmg`, `spell_dmg`, `healing`, `defense`)
VALUES
    ( 1, 1.0, 1.0, 1.0, 1.0),  -- Guerrero       (Warrior)
    ( 2, 1.0, 1.0, 1.0, 1.0),  -- Paladín        (Paladin)
    ( 3, 1.0, 1.0, 1.0, 1.0),  -- Cazador        (Hunter)
    ( 4, 1.0, 1.0, 1.0, 1.0),  -- Pícaro         (Rogue)
    ( 5, 1.0, 1.0, 1.0, 1.0),  -- Sacerdote      (Priest)
    ( 6, 1.0, 1.0, 1.0, 1.0),  -- C. de la Muerte (Death Knight)
    ( 7, 1.0, 1.0, 1.0, 1.0),  -- Chamán         (Shaman)
    ( 8, 1.0, 1.0, 1.0, 1.0),  -- Mago           (Mage)
    ( 9, 1.0, 1.0, 1.0, 1.0),  -- Brujo          (Warlock)
    (11, 1.0, 1.0, 1.0, 1.0);  -- Druida         (Druid)

-- ============================================================
-- Tabla de overrides por hechizo específico
-- ============================================================
-- Permite ajustar el multiplicador de daño o curación de
-- un hechizo concreto (por su spell_id), de forma independiente
-- al multiplicador de clase.
-- Los multiplicadores se APILAN con los de clase:
--   daño final = clase.spell_dmg × spell.dmg_mult
-- ============================================================

CREATE TABLE IF NOT EXISTS `mod_class_balance_spells` (
    `spell_id`   INT UNSIGNED NOT NULL
        COMMENT 'ID del hechizo en spell_dbc',
    `dmg_mult`   FLOAT        NOT NULL DEFAULT 1.0
        COMMENT 'Mult. de daño del hechizo (spell damage y DoTs). 0.8 = -20%',
    `heal_mult`  FLOAT        NOT NULL DEFAULT 1.0
        COMMENT 'Mult. de curación del hechizo. 1.2 = +20%',
    `comment`    VARCHAR(128) NOT NULL DEFAULT ''
        COMMENT 'Descripción opcional (ej: "Bola de fuego - Mago")',
    PRIMARY KEY (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Overrides de balance por hechizo específico (mod-class-balance)';

-- Ejemplos comentados (descomenta y ajusta según necesites):
-- INSERT IGNORE INTO `mod_class_balance_spells` (`spell_id`, `dmg_mult`, `heal_mult`, `comment`)
-- VALUES
--   (133,   0.85, 1.0,  'Bola de fuego (Mago) – reducida 15%'),
--   (48778, 0.90, 1.0,  'Toque de agonía (DK) – reducida 10%'),
--   (48785, 1.0,  1.15, 'Destello de luz (Paladín) – +15% curación');
