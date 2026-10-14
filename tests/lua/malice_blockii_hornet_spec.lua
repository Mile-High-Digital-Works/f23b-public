-- Exercise real payload definitions, pylon choices, presets and quick missions.
_ = function(s) return s end
CAT_AIR_TO_AIR, AIM_120, AIM_9X, AIM_9_CX_PIL = 1, 24, 136, 1.03
WSTYPE_PLACEHOLDER = 'placeholder'
enhanced_a2a_warhead = function(m,c) return {mass=m,caliber=c} end
predefined_warhead = function(n) return {name=n} end
local declared = {}
-- Independent registration: one declaration per project weapon, never a stock name.
declare_weapon = function(w) assert(not declared[w.name]); declared[w.name] = w end
local loads={}
declare_loadout = function(l) assert(not loads[l.CLSID]); loads[l.CLSID]=l end
local root=(os.getenv('F23B_RUNTIME_ROOT') or 'Mods/aircraft')..'/F-23B'
current_mod_path=root
local m=dofile(root..'/Weapons/F23B_AIM424_MALICE.lua')
local s=dofile(root..'/Weapons/F23B_AIM9X_BLOCKII.lua')
assert(m.name=='F23B_AIM424_MALICE' and m.wsTypeOfWeapon[4]==WSTYPE_PLACEHOLDER and declared[m.name]==m)
assert(s.name=='F23B_AIM9X_BLOCKII' and s.wsTypeOfWeapon[4]==WSTYPE_PLACEHOLDER and declared[s.name]==s)
assert(m.shape_table_data[1].name==m.name and s.shape_table_data[1].name==s.name, 'no stock shape identity')
assert(math.abs(m.sensor.FOV-math.rad(15))<1e-12, 'MALICE seeker cone is 15 degrees')
assert(m.M==680.4 and m.M==m.fm.mass and m.fm.caliber==.34)
-- Option C stock-rule identity; all related declarations must change together.
assert(math.abs(m.boost.fuel_mass-386.97363760)<1e-10)
assert(math.abs(m.march.fuel_mass-77.39472752)<1e-10)
assert(m.boost.work_time==15.6 and m.march.work_time==17.55)
assert(m.boost.impulse==280.8 and m.march.impulse==245.7)
assert(m.controller.boost_start==0.4 and m.controller.march_start==39.1)
assert(m.ModelData[16]==15.6 and m.ModelData[17]==17.55)
assert(math.abs(m.ModelData[23]*15.6-m.boost.fuel_mass)<1e-8)
assert(math.abs(m.ModelData[24]*17.55-m.march.fuel_mass)<1e-8)
assert(math.abs(m.ModelData[30]-m.boost.fuel_mass/15.6*9.80665*280.8)<1e-6)
assert(math.abs(m.ModelData[31]-m.march.fuel_mass/17.55*9.80665*245.7)<1e-6)

assert(m.boost.fuel_mass+m.march.fuel_mass<m.M)
assert(m.controller.march_start>=m.controller.boost_start+m.boost.work_time)
assert(m.controller.march_start+m.march.work_time<m.Life_Time)
assert(m.scheme=='aa_missile_amraam2' and m.Head_Type==2 and s.Head_Type==1)
assert(m.model=='F23B_MALICE' and m.shape_table_data[1].file==m.model)
-- Block II motor: project rule of 48.5 kg over 7 s at 262 s specific impulse. Named and solver values agree.
assert(math.abs(s.ModelData[16]*s.ModelData[23]-48.5)<1e-9 and s.ModelData[16]==7 and s.t_acc==s.ModelData[16])
assert(math.abs(s.ModelData[30]-48.5/7*9.80665*262)<1e-6 and s.ModelData[16]*s.ModelData[30]>120000)
assert(s.Life_Time==s.ModelData[36] and s.D_max==11*1852 and s.Range_max==16*1852 and #s.ModelData==59)
assert(s.Fi_search==.09 and s.Fi_start==1.57 and s.Fi_excort==1.57,
    'Hornet seeker contract: retain native search cone and gimbal limits')
assert(m.go_active_by_default==nil and m.send_off_data==nil)
assert(m.sensor.active_radar_lock_dist==m.active_radar_lock_dist)
assert(m.sensor.active_dist_trig_by_default==1)
local malice='{F23B-AIM424-MALICE}'
local ir='{F23B-AIM9X-BLOCKII}'
local amraam='{40EF17B7-F508-45de-8566-6FFECC0C1AB8}'
assert(loads[malice].Weight==m.M and loads[ir].Weight==s.M)
assert(loads[malice].wsTypeOfWeapon==m.wsTypeOfWeapon and loads[ir].wsTypeOfWeapon==s.wsTypeOfWeapon)
MODULATION_AM=0
WOLALIGHT_STROBES,WOLALIGHT_LANDING_LIGHTS,WOLALIGHT_TAXI_LIGHTS=1,2,3
WOLALIGHT_NAVLIGHTS,WOLALIGHT_FORMATION_LIGHTS=4,5
gun_mount=function() return {} end
pylon=function(...) return {...} end
aircraft_task=function(t) return t end
verbose_to_dmg_properties=function(t) return t end
local aircraft
add_aircraft=function(a) aircraft=a end
dofile(root..'/F-23B.lua')
local allowed={}
for n,p in ipairs(aircraft.Pylons) do
    allowed[n]={}
    for _,choice in ipairs(p[7]) do
        allowed[n][choice.CLSID]=true
        assert(choice.CLSID~='{F23B-AIM260A}', 'obsolete B-slot payload must not silently fire MALICE')
        if choice.CLSID==malice then
            assert((n==2 or n==4 or n==6) and p[2]==1 and p[6].eject_dir[2]==-1)
            if n==2 then assert(choice.attach_point_position[1]==.15 and choice.attach_point_position[2]==-.095)
            else assert(choice.attach_point_position[1]==0 and choice.attach_point_position[2]==0) end
            assert(#choice.forbidden==2)
            local excluded={}
            for _,rule in ipairs(choice.forbidden) do
                assert(rule.station~=n and (rule.station==2 or rule.station==4 or rule.station==6))
                assert(not excluded[rule.station]); excluded[rule.station]=true
                assert(#rule.loadout==1 and rule.loadout[1]==amraam, 'must allow neighboring MALICE')
            end
        end
        if choice.CLSID==ir then assert((n==1 or n==9) and p[2]==0) end
    end
end
for _,n in ipairs({2,4,6}) do assert(allowed[n][amraam] and allowed[n][malice]) end
local harm='{F23B-AGM88G}'
for _,n in ipairs({2,3,7}) do assert(allowed[n][harm]) end
for _,n in ipairs({3,7}) do assert(not allowed[n][malice] and not allowed[n][amraam]) end
for _,n in ipairs({4,6}) do assert(not allowed[n][harm]) end
local function check(p)
    local counts={}; local stations={}
    for index,row in pairs(p.pylons) do
        local n=row.num or index
        assert(allowed[n] and allowed[n][row.CLSID], 'invalid payload station '..n)
        assert(not stations[n]); stations[n]=row.CLSID
        counts[row.CLSID]=(counts[row.CLSID] or 0)+1
    end
    if counts[malice] then assert(counts[malice]<=3 and not counts[amraam]) end
    return counts
end
local presets=dofile(root..'/UnitPayloads/F-23B.lua').payloads
assert(check(presets[1])[malice]==3 and check(presets[1])[ir]==2)
assert(check(presets[2])[amraam]==3 and check(presets[2])[ir]==2)
assert(check(presets[3])[ir]==2)
local sead=check(presets[4])
assert(sead[harm]==3 and sead[malice]==2 and sead[ir]==2)
if arg[1] then
    dofile(arg[1]); local players=0
    for _,c in pairs(mission.coalition) do
        for _,country in pairs(c.country or {}) do
            for _,group in pairs((country.plane or {}).group or {}) do
                for _,unit in pairs(group.units or {}) do
                    if unit.type=='F-23B' then
                        assert(check(unit.payload)[malice]==3); players=players+1
                    end
                end
            end
        end
    end
    assert(players==1)
end
print('OFFLINE PASS: three MALICE stations/preset, Block II rails, regular AMRAAM preset')
