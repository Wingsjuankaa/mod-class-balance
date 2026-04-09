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

