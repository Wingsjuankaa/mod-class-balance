#include "ScriptMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ChatCommand.h"
#include "StringFormat.h"
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <cmath>

// ─── Multiplicadores por clase ────────────────────────────────────────────────
struct ClassMultipliers
{
    float physDmg  = 1.0f;
    float spellDmg = 1.0f;
    float healing  = 1.0f;
    float defense  = 1.0f;
};

// ─── Multiplicadores por hechizo específico ───────────────────────────────────
struct SpellMultiplier
{
    float dmgMult  = 1.0f;  // Daño (spell damage y DoTs) del hechizo
    float healMult = 1.0f;  // Curación del hechizo
};

// ─── Manager ──────────────────────────────────────────────────────────────────
class ClassBalanceMgr
{
public:
    static ClassBalanceMgr* instance()
    {
        static ClassBalanceMgr inst;
        return &inst;
    }

    // ── Configuración ──────────────────────────────────────────────────────────
    static bool IsEnabled()
    {
        return sConfigMgr->GetOption<bool>("ClassBalance.Enable", true);
    }

    static bool ApplyInPvP()
    {
        return sConfigMgr->GetOption<bool>("ClassBalance.ApplyInPvP", true);
    }

    static bool ApplyInPvE()
    {
        return sConfigMgr->GetOption<bool>("ClassBalance.ApplyInPvE", true);
    }

    // ── Ayuda: nombre de clase ─────────────────────────────────────────────────
    static char const* GetClassName(uint8 classId)
    {
        switch (classId)
        {
            case  1: return "Guerrero";
            case  2: return "Paladin";
            case  3: return "Cazador";
            case  4: return "Picaro";
            case  5: return "Sacerdote";
            case  6: return "CaballeroMuerte";
            case  7: return "Chaman";
            case  8: return "Mago";
            case  9: return "Brujo";
            case 11: return "Druida";
            default: return "Desconocida";
        }
    }

    // Convierte nombre (inglés o español, sin distinción mayúsculas) a class_id
    static uint8 ClassNameToId(std::string name)
    {
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name == "warrior"      || name == "guerrero")           return  1;
        if (name == "paladin"      || name == "paladin")            return  2;
        if (name == "hunter"       || name == "cazador")            return  3;
        if (name == "rogue"        || name == "picaro"
                                   || name == "pícaro")             return  4;
        if (name == "priest"       || name == "sacerdote")          return  5;
        if (name == "deathknight"  || name == "dk"
                                   || name == "caballerodelamuerte"
                                   || name == "caballeromuerte")    return  6;
        if (name == "shaman"       || name == "chaman"
                                   || name == "chamán")             return  7;
        if (name == "mage"         || name == "mago")               return  8;
        if (name == "warlock"      || name == "brujo")              return  9;
        if (name == "druid"        || name == "druida")             return 11;
        return 0;
    }


    // ── Obtener multiplicadores de una clase ───────────────────────────────────
    ClassMultipliers const& Get(uint8 classId) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _mults.find(classId);
        if (it != _mults.end())
            return it->second;
        static ClassMultipliers defaults{};
        return defaults;
    }

    // ── Carga desde la BD ──────────────────────────────────────────────────────
    void LoadAll()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _mults.clear();
        }

        // Garantizar que existen filas para todas las clases (síncrono)
        static uint8 const classes[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 };
        for (uint8 c : classes)
        {
            WorldDatabase.DirectExecute(
                "INSERT IGNORE INTO `mod_class_balance` "
                "(`class_id`,`phys_dmg`,`spell_dmg`,`healing`,`defense`) "
                "VALUES ({}, 1.0, 1.0, 1.0, 1.0)",
                static_cast<uint32>(c));
        }

        QueryResult result = WorldDatabase.Query(
            "SELECT `class_id`, `phys_dmg`, `spell_dmg`, `healing`, `defense` "
            "FROM `mod_class_balance` ORDER BY `class_id`");

        if (!result)
        {
            LOG_WARN("module",
                "ClassBalance: Tabla mod_class_balance vacía o no existe. "
                "Ejecuta sql/mod_class_balance.sql primero.");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* f = result->Fetch();
            uint8 classId = f[0].Get<uint8>();
            ClassMultipliers m;
            m.physDmg  = f[1].Get<float>();
            m.spellDmg = f[2].Get<float>();
            m.healing  = f[3].Get<float>();
            m.defense  = f[4].Get<float>();
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _mults[classId] = m;
            }
            ++count;
        }
        while (result->NextRow());

        LOG_INFO("module", "ClassBalance: {} clase(s) cargadas.", count);

        // Cargar también los overrides por hechizo
        LoadAllSpells();
    }

    // ── Carga hechizos específicos desde la BD ────────────────────────────────
    void LoadAllSpells()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _spellMults.clear();
        }

        QueryResult result = WorldDatabase.Query(
            "SELECT `spell_id`, `dmg_mult`, `heal_mult` "
            "FROM `mod_class_balance_spells`");

        if (!result)
        {
            LOG_INFO("module", "ClassBalance: Tabla mod_class_balance_spells vacía o sin filas.");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* f = result->Fetch();
            uint32 spellId = f[0].Get<uint32>();
            SpellMultiplier m;
            m.dmgMult  = f[1].Get<float>();
            m.healMult = f[2].Get<float>();
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _spellMults[spellId] = m;
            }
            ++count;
        }
        while (result->NextRow());

        LOG_INFO("module", "ClassBalance: {} hechizo(s) con override cargados.", count);
    }

    // ── Ajusta un multiplicador, guarda en BD y actualiza caché ───────────────
    // type: "phys" | "spell" | "heal" | "def"
    // Devuelve false si el tipo o el classId no son válidos
    bool SetMultiplier(uint8 classId, std::string const& type, float value)
    {
        if (classId == 0 || value < 0.0f || value > 100.0f)
            return false;

        std::string column;
        std::string t = type;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t == "phys"  || t == "fisico"  || t == "melee")    column = "phys_dmg";
        else if (t == "spell" || t == "hechizo" || t == "magia") column = "spell_dmg";
        else if (t == "heal"  || t == "cura" || t == "curacion") column = "healing";
        else if (t == "def"   || t == "defense" || t == "defensa") column = "defense";
        else return false;

        WorldDatabase.DirectExecute(
            "INSERT INTO `mod_class_balance` "
            "(`class_id`,`phys_dmg`,`spell_dmg`,`healing`,`defense`) "
            "VALUES ({},1.0,1.0,1.0,1.0) "
            "ON DUPLICATE KEY UPDATE `{}` = {}",
            static_cast<uint32>(classId), column, value);

        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto& m = _mults[classId];
            if (column == "phys_dmg")  m.physDmg  = value;
            if (column == "spell_dmg") m.spellDmg = value;
            if (column == "healing")   m.healing  = value;
            if (column == "defense")   m.defense  = value;
        }
        return true;
    }

    // ── Resetea una clase a 1.0 ────────────────────────────────────────────────
    void ResetClass(uint8 classId)
    {
        WorldDatabase.DirectExecute(
            "UPDATE `mod_class_balance` SET "
            "`phys_dmg`=1.0, `spell_dmg`=1.0, `healing`=1.0, `defense`=1.0 "
            "WHERE `class_id`={}",
            static_cast<uint32>(classId));
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _mults[classId] = ClassMultipliers{};
        }
    }

    // ── Comprueba si debe aplicarse según el tipo de combate ──────────────────
    // attacker puede ser nullptr (DoTs sin atacante vivo)
    static bool ShouldApply(Unit const* attacker, Unit const* victim)
    {
        if (!attacker || !victim)
            return true; // conservador: sí aplica

        bool attackerIsPlayer = (attacker->GetTypeId() == TYPEID_PLAYER);
        bool victimIsPlayer   = (victim->GetTypeId()   == TYPEID_PLAYER);

        bool isPvP = attackerIsPlayer && victimIsPlayer;
        bool isPvE = attackerIsPlayer && !victimIsPlayer;

        if (isPvP && !ApplyInPvP()) return false;
        if (isPvE && !ApplyInPvE()) return false;
        return true;
    }

    // ── Obtener multiplicador de un hechizo específico ────────────────────────
    SpellMultiplier GetSpellMult(uint32 spellId) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _spellMults.find(spellId);
        if (it != _spellMults.end())
            return it->second;
        return SpellMultiplier{};
    }

    // ── Ajusta un multiplicador de hechizo, guarda en BD y actualiza caché ───
    // type: "dmg" | "heal"
    bool SetSpellMultiplier(uint32 spellId, std::string const& type, float value)
    {
        if (spellId == 0 || value < 0.0f || value > 100.0f)
            return false;

        std::string t = type;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);

        std::string column;
        if (t == "dmg"  || t == "damage" || t == "daño" || t == "dano"
                        || t == "hechizo" || t == "spell")
            column = "dmg_mult";
        else if (t == "heal" || t == "cura" || t == "curacion" || t == "curación")
            column = "heal_mult";
        else
            return false;

        WorldDatabase.DirectExecute(
            "INSERT INTO `mod_class_balance_spells` "
            "(`spell_id`,`dmg_mult`,`heal_mult`) "
            "VALUES ({},1.0,1.0) "
            "ON DUPLICATE KEY UPDATE `{}` = {}",
            spellId, column, value);

        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto& m = _spellMults[spellId];
            if (column == "dmg_mult")  m.dmgMult  = value;
            if (column == "heal_mult") m.healMult = value;
        }
        return true;
    }

    // ── Elimina el override de un hechizo específico ──────────────────────────
    void ResetSpell(uint32 spellId)
    {
        WorldDatabase.DirectExecute(
            "DELETE FROM `mod_class_balance_spells` WHERE `spell_id`={}",
            spellId);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _spellMults.erase(spellId);
        }
    }

private:
    mutable std::mutex _mutex;
    std::unordered_map<uint8,  ClassMultipliers> _mults;
    std::unordered_map<uint32, SpellMultiplier>  _spellMults;
};

#define sClassBalance ClassBalanceMgr::instance()

// ─── Script de mundo (carga al iniciar) ──────────────────────────────────────
class ClassBalanceWorldScript : public WorldScript
{
public:
    ClassBalanceWorldScript() : WorldScript("ClassBalanceWorldScript") {}

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        if (ClassBalanceMgr::IsEnabled())
            sClassBalance->LoadAll();
    }
};

// ─── UnitScript – aplica los multiplicadores ─────────────────────────────────
class ClassBalanceUnitScript : public UnitScript
{
public:
    ClassBalanceUnitScript() : UnitScript("ClassBalanceUnitScript", true,
        { UNITHOOK_MODIFY_MELEE_DAMAGE,
          UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
          UNITHOOK_MODIFY_HEAL_RECEIVED,
          UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK })
    {}

    // ── Daño físico melee/ranged ──────────────────────────────────────────────
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (!ClassBalanceMgr::IsEnabled() || damage == 0)
            return;
        if (!ClassBalanceMgr::ShouldApply(attacker, target))
            return;

        float mult = 1.0f;

        // Multiplicador de daño dado por la clase atacante
        if (attacker && attacker->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(attacker->getClass()).physDmg;

        // Multiplicador de daño recibido por la clase víctima
        if (target && target->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(target->getClass()).defense;

        if (std::fabs(mult - 1.0f) > 0.001f)
            damage = static_cast<uint32>(static_cast<float>(damage) * mult);
    }

    // ── Daño de hechizo ───────────────────────────────────────────────────────
    void ModifySpellDamageTaken(Unit* target, Unit* attacker,
                                int32& damage, SpellInfo const* spellInfo) override
    {
        if (!ClassBalanceMgr::IsEnabled() || damage <= 0)
            return;
        if (!ClassBalanceMgr::ShouldApply(attacker, target))
            return;

        float mult = 1.0f;

        if (attacker && attacker->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(attacker->getClass()).spellDmg;

        if (target && target->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(target->getClass()).defense;

        // Override por hechizo específico (se apila con el multiplicador de clase)
        if (spellInfo)
        {
            float spellSpecific = sClassBalance->GetSpellMult(spellInfo->Id).dmgMult;
            if (std::fabs(spellSpecific - 1.0f) > 0.001f)
                mult *= spellSpecific;
        }

        if (std::fabs(mult - 1.0f) > 0.001f)
            damage = static_cast<int32>(static_cast<float>(damage) * mult);
    }

    // ── Daño periódico (DoTs) ─────────────────────────────────────────────────
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker,
                                       uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!ClassBalanceMgr::IsEnabled() || damage == 0)
            return;
        if (!ClassBalanceMgr::ShouldApply(attacker, target))
            return;

        float mult = 1.0f;

        if (attacker && attacker->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(attacker->getClass()).spellDmg;

        if (target && target->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(target->getClass()).defense;

        // Override por hechizo específico
        if (spellInfo)
        {
            float spellSpecific = sClassBalance->GetSpellMult(spellInfo->Id).dmgMult;
            if (std::fabs(spellSpecific - 1.0f) > 0.001f)
                mult *= spellSpecific;
        }

        if (std::fabs(mult - 1.0f) > 0.001f)
            damage = static_cast<uint32>(static_cast<float>(damage) * mult);
    }

    // ── Curación ──────────────────────────────────────────────────────────────
    void ModifyHealReceived(Unit* target, Unit* healer,
                            uint32& heal, SpellInfo const* spellInfo) override
    {
        if (!ClassBalanceMgr::IsEnabled() || heal == 0)
            return;

        float mult = 1.0f;

        if (healer && healer->GetTypeId() == TYPEID_PLAYER)
            mult *= sClassBalance->Get(healer->getClass()).healing;

        // Override por hechizo específico
        if (spellInfo)
        {
            float spellSpecific = sClassBalance->GetSpellMult(spellInfo->Id).healMult;
            if (std::fabs(spellSpecific - 1.0f) > 0.001f)
                mult *= spellSpecific;
        }

        if (std::fabs(mult - 1.0f) > 0.001f)
            heal = static_cast<uint32>(static_cast<float>(heal) * mult);
    }
};

// ─── CommandScript ────────────────────────────────────────────────────────────
// .classbalance list
// .classbalance info <clase>
// .classbalance set <clase> <tipo> <valor>
// .classbalance reset <clase>
// .classbalance reload
// .classbalance spell list
// .classbalance spell info <spell_id>
// .classbalance spell set <spell_id> <dmg|heal> <valor>
// .classbalance spell reset <spell_id>
using namespace Acore::ChatCommands;

class ClassBalanceCommandScript : public CommandScript
{
public:
    ClassBalanceCommandScript() : CommandScript("ClassBalanceCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable spellSub =
        {
            { "list",  HandleCBSpellList,  SEC_GAMEMASTER,    Console::No },
            { "info",  HandleCBSpellInfo,  SEC_GAMEMASTER,    Console::No },
            { "set",   HandleCBSpellSet,   SEC_ADMINISTRATOR, Console::No },
            { "reset", HandleCBSpellReset, SEC_ADMINISTRATOR, Console::No },
        };
        static ChatCommandTable sub =
        {
            { "list",   HandleCBList,   SEC_GAMEMASTER,    Console::No },
            { "info",   HandleCBInfo,   SEC_GAMEMASTER,    Console::No },
            { "set",    HandleCBSet,    SEC_ADMINISTRATOR, Console::No },
            { "reset",  HandleCBReset,  SEC_ADMINISTRATOR, Console::No },
            { "reload", HandleCBReload, SEC_ADMINISTRATOR, Console::No },
            { "spell",  spellSub },
        };
        static ChatCommandTable root = { { "classbalance", sub } };
        return root;
    }

    // .classbalance list
    static bool HandleCBList(ChatHandler* handler)
    {
        if (!ClassBalanceMgr::IsEnabled())
        {
            handler->SendSysMessage(
                "|cffff4444[ClassBalance]|r Módulo desactivado.");
            return true;
        }

        static uint8 const classes[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 };

        handler->SendSysMessage(
            "|cffffff00[ClassBalance]|r Multiplicadores actuales:");
        handler->SendSysMessage(
            "|cffaaaaaa  Clase              Físico  Hechizo  Curación  Defensa|r");

        for (uint8 c : classes)
        {
            auto const& m = sClassBalance->Get(c);
            handler->PSendSysMessage(
                "  |cff00ccff{:<17}|r  {:.2f}   {:.2f}    {:.2f}     {:.2f}",
                ClassBalanceMgr::GetClassName(c),
                m.physDmg, m.spellDmg, m.healing, m.defense);
        }

        handler->SendSysMessage(
            "|cffaaaaaa Usa .classbalance set <clase> <phys|spell|heal|def> <valor>|r");
        return true;
    }

    // .classbalance info <clase>
    static bool HandleCBInfo(ChatHandler* handler, std::string const& className)
    {
        uint8 classId = ClassBalanceMgr::ClassNameToId(className);
        if (classId == 0)
        {
            handler->PSendSysMessage(
                "|cffff4444[ClassBalance]|r Clase '{}' desconocida.", className);
            return false;
        }

        auto const& m = sClassBalance->Get(classId);

        handler->PSendSysMessage(
            "|cffffff00[ClassBalance] {}|r (ID: {})",
            ClassBalanceMgr::GetClassName(classId), classId);
        handler->PSendSysMessage(
            "  Daño físico dado   : |cff{}|r{:.2f}x",
            (m.physDmg > 1.0f ? "00ff00" : (m.physDmg < 1.0f ? "ff4444" : "ffffff")),
            m.physDmg);
        handler->PSendSysMessage(
            "  Daño hechizo dado  : |cff{}|r{:.2f}x",
            (m.spellDmg > 1.0f ? "00ff00" : (m.spellDmg < 1.0f ? "ff4444" : "ffffff")),
            m.spellDmg);
        handler->PSendSysMessage(
            "  Curación hecha     : |cff{}|r{:.2f}x",
            (m.healing > 1.0f ? "00ff00" : (m.healing < 1.0f ? "ff4444" : "ffffff")),
            m.healing);
        handler->PSendSysMessage(
            "  Daño recibido (def): |cff{}|r{:.2f}x  |cffaaaaaa(menor = más defensa)|r",
            (m.defense < 1.0f ? "00ff00" : (m.defense > 1.0f ? "ff4444" : "ffffff")),
            m.defense);
        return true;
    }

    // .classbalance set <clase> <tipo> <valor>
    static bool HandleCBSet(ChatHandler* handler,
                            std::string const& className,
                            std::string const& type,
                            float value)
    {
        uint8 classId = ClassBalanceMgr::ClassNameToId(className);
        if (classId == 0)
        {
            handler->PSendSysMessage(
                "|cffff4444[ClassBalance]|r Clase '{}' desconocida.", className);
            return false;
        }

        if (value < 0.0f || value > 100.0f)
        {
            handler->SendSysMessage(
                "|cffff4444[ClassBalance]|r El valor debe estar entre 0.0 y 100.0.");
            return false;
        }

        if (!sClassBalance->SetMultiplier(classId, type, value))
        {
            handler->PSendSysMessage(
                "|cffff4444[ClassBalance]|r Tipo '{}' no válido. "
                "Usa: phys | spell | heal | def.", type);
            return false;
        }

        handler->PSendSysMessage(
            "|cff00ff00[ClassBalance]|r |cffffff00{}|r > {}: "
            "|cffff8800{:.2f}x|r  (guardado en BD)",
            ClassBalanceMgr::GetClassName(classId), type, value);
        return true;
    }

    // .classbalance reset <clase>
    static bool HandleCBReset(ChatHandler* handler, std::string const& className)
    {
        uint8 classId = ClassBalanceMgr::ClassNameToId(className);
        if (classId == 0)
        {
            handler->PSendSysMessage(
                "|cffff4444[ClassBalance]|r Clase '{}' desconocida.", className);
            return false;
        }

        sClassBalance->ResetClass(classId);

        handler->PSendSysMessage(
            "|cff00ff00[ClassBalance]|r |cffffff00{}|r reseteada a 1.0x en todos los tipos.",
            ClassBalanceMgr::GetClassName(classId));
        return true;
    }

    // .classbalance reload
    static bool HandleCBReload(ChatHandler* handler)
    {
        sClassBalance->LoadAll();
        handler->SendSysMessage(
            "|cff00ff00[ClassBalance]|r Multiplicadores recargados desde la BD.");
        return true;
    }

    // ── .classbalance spell list ───────────────────────────────────────────────
    static bool HandleCBSpellList(ChatHandler* handler)
    {
        if (!ClassBalanceMgr::IsEnabled())
        {
            handler->SendSysMessage("|cffff4444[ClassBalance]|r Módulo desactivado.");
            return true;
        }

        QueryResult result = WorldDatabase.Query(
            "SELECT `spell_id`, `dmg_mult`, `heal_mult`, `comment` "
            "FROM `mod_class_balance_spells` ORDER BY `spell_id`");

        if (!result)
        {
            handler->SendSysMessage(
                "|cffffff00[ClassBalance]|r No hay hechizos con override configurado.");
            return true;
        }

        handler->SendSysMessage("|cffffff00[ClassBalance]|r Overrides por hechizo:");
        handler->SendSysMessage(
            "|cffaaaaaa  SpellID     Daño   Curación  Comentario|r");

        do
        {
            Field* f = result->Fetch();
            uint32      spellId = f[0].Get<uint32>();
            float       dmg     = f[1].Get<float>();
            float       heal    = f[2].Get<float>();
            std::string comment = f[3].Get<std::string>();

            handler->PSendSysMessage(
                "  |cff00ccff{:<10}|r  {:.2f}   {:.2f}    {}",
                spellId, dmg, heal,
                comment.empty() ? "-" : comment);
        }
        while (result->NextRow());

        handler->SendSysMessage(
            "|cffaaaaaa Usa .classbalance spell set <id> <dmg|heal> <valor>|r");
        return true;
    }

    // ── .classbalance spell info <spell_id> ───────────────────────────────────
    static bool HandleCBSpellInfo(ChatHandler* handler, uint32 spellId)
    {
        if (spellId == 0)
        {
            handler->SendSysMessage("|cffff4444[ClassBalance]|r spell_id inválido.");
            return false;
        }

        auto const m = sClassBalance->GetSpellMult(spellId);
        bool hasOverride = (std::fabs(m.dmgMult - 1.0f) > 0.001f ||
                            std::fabs(m.healMult - 1.0f) > 0.001f);

        handler->PSendSysMessage(
            "|cffffff00[ClassBalance]|r Hechizo |cff00ccff{}|r:{}",
            spellId, hasOverride ? "" : "  |cffaaaaaa(sin override – valores por defecto)|r");

        handler->PSendSysMessage(
            "  Daño      : |cff{}|r{:.2f}x",
            (m.dmgMult > 1.0f ? "00ff00" : (m.dmgMult < 1.0f ? "ff4444" : "ffffff")),
            m.dmgMult);
        handler->PSendSysMessage(
            "  Curación  : |cff{}|r{:.2f}x",
            (m.healMult > 1.0f ? "00ff00" : (m.healMult < 1.0f ? "ff4444" : "ffffff")),
            m.healMult);

        return true;
    }

    // ── .classbalance spell set <spell_id> <tipo> <valor> ─────────────────────
    static bool HandleCBSpellSet(ChatHandler* handler,
                                 uint32 spellId,
                                 std::string const& type,
                                 float value)
    {
        if (spellId == 0)
        {
            handler->SendSysMessage("|cffff4444[ClassBalance]|r spell_id inválido.");
            return false;
        }

        if (value < 0.0f || value > 100.0f)
        {
            handler->SendSysMessage(
                "|cffff4444[ClassBalance]|r El valor debe estar entre 0.0 y 100.0.");
            return false;
        }

        if (!sClassBalance->SetSpellMultiplier(spellId, type, value))
        {
            handler->PSendSysMessage(
                "|cffff4444[ClassBalance]|r Tipo '{}' no válido. Usa: dmg | heal.", type);
            return false;
        }

        handler->PSendSysMessage(
            "|cff00ff00[ClassBalance]|r Hechizo |cff00ccff{}|r > {}: "
            "|cffff8800{:.2f}x|r  (guardado en BD)",
            spellId, type, value);
        return true;
    }

    // ── .classbalance spell reset <spell_id> ──────────────────────────────────
    static bool HandleCBSpellReset(ChatHandler* handler, uint32 spellId)
    {
        if (spellId == 0)
        {
            handler->SendSysMessage("|cffff4444[ClassBalance]|r spell_id inválido.");
            return false;
        }

        sClassBalance->ResetSpell(spellId);

        handler->PSendSysMessage(
            "|cff00ff00[ClassBalance]|r Override del hechizo |cff00ccff{}|r eliminado.",
            spellId);
        return true;
    }
};

// ─── Registro ─────────────────────────────────────────────────────────────────
void AddSC_mod_class_balance()
{
    new ClassBalanceWorldScript();
    new ClassBalanceUnitScript();
    new ClassBalanceCommandScript();
}

