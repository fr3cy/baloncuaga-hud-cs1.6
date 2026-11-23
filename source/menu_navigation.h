#pragma once
// Shared Aurora page registry, section search and navigation history.
struct PeachPage {const char *name,*english,*keywords;int tab,sub;};
static const PeachPage peachPages[]={
    {u8"Модели / Нож","Models / Knife","knife model",0,0},{u8"Модели / HE-граната","Models / HE grenade","grenade model",0,1},
    {u8"Модели / Флешка","Models / Flashbang","flash model",0,2},{u8"Модели / Дымовая","Models / Smoke","smoke model",0,3},
    {u8"Модели / Одежда команд","Models / Team outfits",u8"одежда кт тт ct tt terror hoodie hood vetements enfants rick owens mowalola stomper geobasket костюм",0,4},
    {u8"Камера / FOV","Camera / FOV",u8"поле зрения camera",1,0},{u8"Прицел","Crosshair",u8"crosshair цвет размер точка обводка",2,0},
    {u8"Чат / Настройки","Chat / Settings",u8"chat css сообщения размер текст ширина строки theme font size",5,0},{u8"HUD / Радар","HUD / Radar",u8"radar обводка карта zoom стрелка arrow здание внутри крыша building inside roof",3,0},
    {"HUD / FOG","HUD / FOG",u8"speedometer спидометр style стиль frame on ground bhop duck sgs gain прыжки прирост glow свечение цвет",3,1},
    {u8"HUD / Игровой HUD","HUD / In-game HUD",u8"health здоровье hp броня armor ammo патроны money деньги таймер timer прозрачность transparency",3,2},
    {u8"HUD / Небо","HUD / Skybox",u8"скайбокс sky skybox небо солнце sunny ночь night звёзды stars закат sunset звезды stars aurora космос space",3,3},
    {u8"Тренажёр колеса","Scroll trainer",u8"scroll trainer статистика",4,0},{u8"Чат / История","Chat / History",u8"chat history time server dead",5,1},
    {u8"Настройки / Общие","Settings / General",u8"peach framework aurora тема theme classic дым smoke general флешка флеш вспышка ослепление flash flashbang",6,2},
    {u8"Misc / Конфигурация","Misc / Configuration",u8"config save load профиль сохранить загрузить",7,0},
    {"Showkeys","Showkeys",u8"клавиши keys wasd показ кнопки",9,0},
    {u8"Настройки / Оформление","Settings / Appearance",u8"палитра palette цвет color язык language english русский масштаб scale клавиша key логотип",6,0},
    {u8"Настройки / Ватермарк","Settings / Watermark",u8"watermark ватермарк fps пинг ping ник nick плашка",6,1},
    {u8"Настройки / Экран","Settings / Screen",u8"время часовой пояс timezone time zone utc spectator наблюдатель чёрные полосы black bars",6,3},
    {u8"Настройки / Анимация","Settings / Motion",u8"анимация animated opening closing orbit aurora particles частицы свечение контур плавность шаблон",6,4},
    {"Stats","Stats",u8"статистика fog bhop duck sgs jumpbug ideal distr проценты",8,0}
};
static const char *peachName(const PeachPage &page){return Lang::english()?page.english:page.name;}struct PeachVisit {int tab,sub;};
static std::vector<PeachVisit> peachVisits;
static int peachVisit=-1;
static char peachSearch[96];
static std::wstring peachFold(const char *text){
    int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0);if(n<=1)return {};
    std::wstring out(n,L'\0');MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),n);out.resize(n-1);CharLowerBuffW(out.data(),out.size());return out;
}
static bool peachMatches(const PeachPage &page,const char *query){
    auto text=peachFold((std::string(page.name)+" "+page.english+" "+page.keywords).c_str());return text.find(peachFold(query))!=std::wstring::npos;
}
static void peachNavigate(int tab,int sub,bool remember=true){
    if(tab<0||tab>=static_cast<int>(ui::tabs.size()))return;
    sub=std::clamp(sub,0,std::max(0,static_cast<int>(ui::tabs[tab].pages.size())-1));
    if(remember&&(peachVisit<0||peachVisits[peachVisit].tab!=tab||peachVisits[peachVisit].sub!=sub)){
        peachVisits.resize(peachVisit+1);peachVisits.push_back({tab,sub});
        if(peachVisits.size()>32)peachVisits.erase(peachVisits.begin());peachVisit=static_cast<int>(peachVisits.size())-1;
    }
    ui::cur_tab=ui::next_tab=tab;ui::tabs[tab].cur_subtab=ui::tabs[tab].next_subtab=sub;
    ui::anim=ui::anim_dst=ui::content_anim=ui::content_anim_dst=1;
}
