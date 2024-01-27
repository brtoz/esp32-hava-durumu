Bu projede, ESP32 ve Nextion ekranı kullanarak OpenWeatherMap üzerinden günlük, saatlik ve hava kalitesi verilerini alıyoruz. Bu verileri Nextion ekranında gösteriyoruz. Ayrıca belirli bir hava durumu koşulu gerçekleştiğinde uyarı e-postası gönderebiliyoruz.

![Sema](sema.gif)
(Ana Sayfaya e-posta görseli ekledim mail başarıyla gönderildiğinde yeşile dönüyor.)

Projede Kullanılan Devre Elemanları:<br>
ESP32 Wifi + Bluetooth Dual-Mode Geliştirme Kartı <br>
NX3224T024 - 2.4 Inch Nextion Dokunmatik Lcd Ekran<br>
GY-NEO6MV2 GPS Modülü<br>
DS1302 RTC Modülü<br>
Tek Kanal 5 V Röle Kartı<br>
Mini Breadboard<br>
Jumper Kablo

Kurulum:<br>
ESP32 ve Nextion bağlantılarını şemadaki gibi yapın.<br>
OpenWeatherMap'ten API anahtarı alın. (⚠️ Kodun düzgün çalışması için Geliştirici Paketine sahip olmanız gerekmektedir.)<br>
ESP32'nizi Wi-Fi ağınıza bağlayın.<br>
Hava durumu verilerini almak için ESP32'nizi OpenWeatherMap API'si ile programlayın.<br>
Alınan verileri Nextion ekranında göstermek için ESP32'nizi programlayın.<br>
Belirli bir hava durumu koşulu gerçekleştiğinde e-posta göndermek için ESP32'nizi programlayın.<br>

Katkıda Bulunma:<br>
Bu proje açık kaynaklıdır ve katkılarınıza açıktır.Kodu kullanmak için bir sebep bulup ve hata gördüyseniz, Issue veya Pull Request açabilirsiniz.
