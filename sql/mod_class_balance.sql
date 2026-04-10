-- ============================================================
-- mod-class-balance: Ajuste de multiplicadores por clase
-- Base de datos: acore_world (WorldDatabase)
-- ============================================================
-- Idempotente: usa CREATE TABLE IF NOT EXISTS + INSERT IGNORE
-- ============================================================

CREATE TABLE IF NOT EXISTS `mod_class_balance` (
    `class_id`    TINYINT UNSIGNED NOT NULL
        COMMENT '1=Guerrero 2=Paladín 3=Cazador 4=Pícaro 5=Sacerdote 6=C.Muerte 7=Chamán 8=Mago 9=Brujo 11=Druida',
    `phys_dmg`    FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. daño físico dado (melee/ranged). 1.2 = +20%',
    `spell_dmg`   FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. daño de hechizo dado (spells, DoTs). 0.9 = -10%',
    `healing`     FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. curación hecha. 1.1 = +10%',
    `defense`     FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. daño recibido (todas las fuentes). 0.85 = 15% menos daño',
    `threat_mult` FLOAT NOT NULL DEFAULT 1.0
        COMMENT 'Mult. amenaza generada en PvE. 1.5 = +50% más aggro (solo afecta a jugadores atacando mobs)',
    PRIMARY KEY (`class_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Multiplicadores de balance por clase de jugador (mod-class-balance)';

-- Valores iniciales: todas las clases en 1.0 (sin modificación)
INSERT IGNORE INTO `mod_class_balance`
    (`class_id`, `phys_dmg`, `spell_dmg`, `healing`, `defense`, `threat_mult`)
VALUES
    ( 1, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Guerrero       (Warrior)
    ( 2, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Paladín        (Paladin)
    ( 3, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Cazador        (Hunter)
    ( 4, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Pícaro         (Rogue)
    ( 5, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Sacerdote      (Priest)
    ( 6, 1.0, 1.0, 1.0, 1.0, 1.0),  -- C. de la Muerte (Death Knight)
    ( 7, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Chamán         (Shaman)
    ( 8, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Mago           (Mage)
    ( 9, 1.0, 1.0, 1.0, 1.0, 1.0),  -- Brujo          (Warlock)
    (11, 1.0, 1.0, 1.0, 1.0, 1.0);  -- Druida         (Druid)

-- ============================================================
-- Migración para instalaciones existentes (compatible MySQL 5.7+)
-- ============================================================
SET @cb_col1 = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME   = 'mod_class_balance'
    AND COLUMN_NAME  = 'threat_mult');
SET @cb_sql1 = IF(@cb_col1 = 0,
    'ALTER TABLE `mod_class_balance` ADD COLUMN `threat_mult` FLOAT NOT NULL DEFAULT 1.0 COMMENT ''Mult. amenaza generada en PvE. 1.5 = +50% mas aggro''',
    'SELECT 1');
PREPARE cb_stmt1 FROM @cb_sql1;
EXECUTE cb_stmt1;
DEALLOCATE PREPARE cb_stmt1;

-- ============================================================
-- Tabla de overrides por hechizo específico
-- ============================================================
CREATE TABLE IF NOT EXISTS `mod_class_balance_spells` (
    `spell_id`    INT UNSIGNED NOT NULL
        COMMENT 'ID del hechizo en spell_dbc',
    `dmg_mult`    FLOAT        NOT NULL DEFAULT 1.0
        COMMENT 'Mult. de daño del hechizo (spell damage y DoTs). 0.8 = -20%',
    `heal_mult`   FLOAT        NOT NULL DEFAULT 1.0
        COMMENT 'Mult. de curación del hechizo. 1.2 = +20%',
    `threat_mult` FLOAT        NOT NULL DEFAULT 1.0
        COMMENT 'Mult. de amenaza del hechizo en PvE. 2.0 = doble amenaza',
    `comment`     VARCHAR(128) NOT NULL DEFAULT ''
        COMMENT 'Descripción opcional (ej: "Golpe de Escudo - Guerrero Prot")',
    PRIMARY KEY (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Overrides de balance por hechizo específico (mod-class-balance)';

-- Migración para instalaciones existentes
SET @cb_col2 = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME   = 'mod_class_balance_spells'
    AND COLUMN_NAME  = 'threat_mult');
SET @cb_sql2 = IF(@cb_col2 = 0,
    'ALTER TABLE `mod_class_balance_spells` ADD COLUMN `threat_mult` FLOAT NOT NULL DEFAULT 1.0 COMMENT ''Mult. de amenaza del hechizo en PvE. 2.0 = doble amenaza''',
    'SELECT 1');
PREPARE cb_stmt2 FROM @cb_sql2;
EXECUTE cb_stmt2;
DEALLOCATE PREPARE cb_stmt2;

-- ============================================================
-- Ejemplos comentados – habilidades de tanque
-- Descomenta las que quieras activar:
-- ============================================================
-- INSERT IGNORE INTO `mod_class_balance_spells`
--     (`spell_id`, `dmg_mult`, `heal_mult`, `threat_mult`, `comment`)
-- VALUES
--   -- Guerrero Protección
--   (23922, 1.0, 1.0, 1.5, 'Golpe de Escudo – +50% amenaza'),
--   (11600, 1.0, 1.0, 1.3, 'Devastar – +30% amenaza'),
--   (1715,  1.0, 1.0, 1.3, 'Ataque Heroico – +30% amenaza'),
--   -- Paladín Protección
--   (48817, 1.0, 1.0, 1.5, 'Escudo Sagrado – +50% amenaza'),
--   (53595, 1.0, 1.0, 1.3, 'Juicio (Prot) – +30% amenaza'),
--   -- Caballero de la Muerte Sangre/Escarcha
--   (49998, 1.0, 1.0, 1.3, 'Golpe de Muerte – +30% amenaza'),
--   (45462, 1.0, 1.0, 1.5, 'Pestilencia – +50% amenaza'),
--   -- Druida Guardián
--   (6807,  1.0, 1.0, 1.3, 'Manotazo – +30% amenaza'),
--   (779,   1.0, 1.0, 1.3, 'Swipe (Oso) – +30% amenaza');
