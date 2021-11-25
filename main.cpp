#include "graph_editor.h"
#include "imgui_window_sdl.h"
#include "imgui_extras.h"
#include "imfilebrowser.h"
#include "thread.h"
#include "timer.h"
#include <GL/glew.h>
#include <math.h>
#include "processing-lib/ocio_processor.h"
#include "socket_notifier.h"

#include "scope-lib/waveform_monitor.h"
#include "processing-lib/lens_correction.h"
#include "raw-lib/mlv_video.h"
#include "raw-lib/dng_video.h"

#include "texture2D.h"

class TestWidget : public Widget
{
	waveformMonitor _wfm;
	TextureRGBA16F* _tex;

	int _cf=0;
	float _idt_mat[9];
	Lens_correction *_lc;
	imgui_addons::ImGuiFileBrowser _fbw;
public:
	//Mlv_video* _video;
   Dng_video* _video;
	TestWidget(Window_SDL* win) : Widget(win, "TestWidget"), _wfm(256,256), _video(NULL)
	{

      //_video = new Mlv_video("/storage/VIDEO/MISC/MLV/M18-1008.MLV");
      _video = new Dng_video("/storage/VIDEO/MISC/RAW/VIDEO_DNG/M02-1705_1_2021-05-02_0001_C0000/M02-1705_1_2021-05-02_0001_C0000_000000.dng");
		//set_maximized(true);
		set_position(50, 50);
		set_size(500,500);
		//set_resizable(false);
		set_titlebar(false);
		uint32_t size;
		int w = _video->raw_resolution_x();
		int h = _video->raw_resolution_y();

		_tex = new TextureRGBA16F(GL_RGB, GL_UNSIGNED_SHORT, w, h, NULL);

      reload();
	}

   void reload(){
      _video->get_frame_as_gl_texture(_cf, *_tex);
   }

	~TestWidget(){
	}

	void draw() override {

      if (_video->need_refresh()){
         _video->get_frame_as_gl_texture(_cf, *_tex);
      }

      ImVec2 winsize = ImGui::GetContentRegionAvail();
		bool open = ImGui::Button("Test", ImVec2(100,20));

		// Same name for openpopup and showFileDialog....
		if(open)ImGui::OpenPopup("Open File");
		if(_fbw.showFileDialogPopup("Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(600, 300), ".mlv,.dng")){
         _cf = 0;
         delete _video;
         delete _tex;
         //_video = new Mlv_video(_fbw.selected_path);
         _video = new Dng_video(_fbw.selected_path);
         
         int w = _video->raw_resolution_x();
		   int h = _video->raw_resolution_y();
		   _tex = new TextureRGBA16F(GL_RGB, GL_UNSIGNED_SHORT, w, h, NULL);
         _video->get_frame_as_gl_texture(_cf, *_tex);
      }

		float ratio = (float)_tex->height() / (float)_tex->width();

		ImGui::Image((void*)(unsigned long)_tex->gl_texture(), ImVec2(winsize.x,int((float)winsize.x*ratio)));
      ImGui::SetNextItemWidth(winsize.x);
      if(ImGui::SliderInt("##frame", &_cf, 0, _video->frame_count() - 1, NULL)){
         _video->set_dirty();
      }
      ImGui::Image((void*)(unsigned long)_video->get_waveform_texture(), ImVec2(512, 256));
	}
};

class RawInfoWidget : public Widget
{
   TestWidget* _video;
   int _sel_camera_model  = 0;
   int _sel_lens_model  = 0;
   std::vector<std::string> _camera_models;
   std::vector<std::string> _lens_models;
   imgui_addons::ImGuiFileBrowser _fbw;   
   float _min_focal, _max_focal;
   bool  _use_lens_id = false;
public:
   RawInfoWidget(Window_SDL* win, TestWidget* video) : Widget(win, "RawInfo"), _video(video)
   {
	   set_size(400,500);
      set_titlebar(false);
      ChildWidget* test = new ChildWidget(this, "Test1", false, 50,0);
      ChildWidget* test2 = new ChildWidget(this, "Test2", true);
      auto_set_lens();
   }

   void update_lenses(){
      _lens_models = Lens_correction::get_lens_models(get_selected_camera());
   }

   void auto_set_lens(){
      std::string camera = _video->_video->camera_name();
      std::string lens = _video->_video->lens_name();
      lens.erase(std::remove(lens.begin(), lens.end(), ' '), lens.end());

      _camera_models = Lens_correction::get_camera_models();    
      _lens_models = Lens_correction::get_lens_models(camera);

      std::vector<std::string>::iterator cam_it = std::find(_camera_models.begin(), _camera_models.end(), camera);
      std::vector<std::string>::iterator lens_it = _lens_models.begin();

      for(;lens_it != _lens_models.end();++lens_it){
         std::string currentlens = *lens_it;
         currentlens.erase(std::remove(currentlens.begin(), currentlens.end(), ' '), currentlens.end());
         if (currentlens.find(lens) != std::string::npos){
            break;
         }
      }

      if (lens_it == _lens_models.end() || _use_lens_id){
         lens = _video->_video->lens_name_by_id();
         lens_it = std::find(_lens_models.begin(), _lens_models.end(), lens);
      }

      if (cam_it == _camera_models.end()){
         _sel_camera_model = -1;
      } else {
         _sel_camera_model = cam_it - _camera_models.begin();
      }

      if (lens_it == _lens_models.end()){
         _sel_lens_model = -1;
      } else {
         _sel_lens_model =  lens_it - _lens_models.begin();
      }

      _video->_video->raw_settings().crop_factor = _video->_video->final_crop_factor();
      _video->_video->raw_settings().focal_length = _video->_video->focal_length();

      Lens_correction::get_lens_min_max_focal(get_selected_lens(), _min_focal, _max_focal);
   }

   std::string get_selected_camera(){
      if (_sel_camera_model >= _camera_models.size()){
         return "";
      }
      return _camera_models[_sel_camera_model];
   }

   std::string get_selected_lens(){
      if (_sel_lens_model >= _lens_models.size()){
         return "";
      }
      return _lens_models[_sel_lens_model];
   }

   void draw() override {
      
      if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
      {
         if (ImGui::BeginTabItem("Raw info"))
         {
            float shutter = 1000.f / float(_video->_video->shutter_speed());
            ImGui::BeginTable("RawTable", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV);
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,   0.0f, 0);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,   0.0f, 1);
            ImGui::TableNextColumn();ImGui::Text("Camera");
            ImGui::TableNextColumn();ImGui::Text(_video->_video->camera_name().c_str());
            ImGui::TableNextColumn();ImGui::Text("ISO");
            ImGui::TableNextColumn();ImGui::Text("%u", _video->_video->iso());
            ImGui::TableNextColumn();ImGui::Text("Shutter speed");
            ImGui::TableNextColumn();ImGui::Text("1/%.2fs", shutter);
            ImGui::TableNextColumn();ImGui::Text("Crop factor");
            ImGui::TableNextColumn();ImGui::Text("%.2f (%.2f)", _video->_video->crop_factor(), _video->_video->final_crop_factor());
            ImGui::TableNextColumn();ImGui::Text("Frames");
            ImGui::TableNextColumn();ImGui::Text("%i", _video->_video->frame_count());
            ImGui::TableNextColumn();ImGui::Text("Frame rate");
            ImGui::TableNextColumn();ImGui::Text("%.2f/s", _video->_video->fps());
            ImGui::TableNextColumn();ImGui::Text("Resolution");
            ImGui::TableNextColumn();ImGui::Text("%ix%i px (%ix%i)", _video->_video->raw_resolution_x(), _video->_video->raw_resolution_y(),
                                                 _video->_video->resolution_x(), _video->_video->raw_resolution_y());
            ImGui::TableNextColumn();ImGui::Text("Depth");
            ImGui::TableNextColumn();ImGui::Text("%i bpp", _video->_video->bpp());
            ImGui::TableNextColumn();ImGui::Text("Sampling");
            ImGui::TableNextColumn();ImGui::Text("%i x %i (%i x %i binning)", _video->_video->sampling_factor_x(), _video->_video->sampling_factor_y(),
                                                 _video->_video->pixel_binning_x(), _video->_video->pixel_binning_y());
            ImGui::EndTable();
            ImGui::EndTabItem();
         }

         if (ImGui::BeginTabItem("Raw tweaks"))
         {
            bool raw_changed = false;
            static int tweak_color_temp;
            if (ImGui::CollapsingHeader("Raw settings", ImGuiTreeNodeFlags_DefaultOpen)) {
               ImGui::BeginTable("Tweak table", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV);

               ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,   0.0f, 0);
               ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,   0.0f, 1);

               ImGui::TableNextColumn();ImGui::Text("Fix focus pixels");
               ImGui::TableNextColumn();raw_changed |= ImGui::Checkbox("##focuspixel", &_video->_video->raw_settings().fix_focuspixels);

               ImGui::TableNextColumn();ImGui::Text("Interpolation");
               ImGui::TableNextColumn();raw_changed |= ImGui::Combo("##dabayering", &_video->_video->raw_settings().interpolation, "None\0Linear\0VNG\0PPG\0AHD\0DCB\0");

               ImGui::TableNextColumn();ImGui::Text("Chroma smooth");
               ImGui::TableNextColumn();raw_changed |= ImGui::Combo("##chromasmooth", &_video->_video->raw_settings().chroma_smooth, "[None]\0[2 x 2]\0[3 x 3]\0[4 x 4]\0[5 x 5]\0");

               ImGui::TableNextColumn();ImGui::Text("Highlight");
               ImGui::TableNextColumn();raw_changed |= ImGui::Combo("##highlight", &_video->_video->raw_settings().highlight, "Clip\0Unclip\0Blend\0Rebuild\0");

               ImGui::TableNextColumn();ImGui::Text("Color temperature");
               ImGui::TableNextColumn();


               //ImGui::SetNextItemWidth(200);
               ImGui::BeginGroup();
               ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x/2 - 60);
               raw_changed |= ImGui::Combo("##colortempmode", &tweak_color_temp, "[Auto]\0[User]\0");
               if(raw_changed && tweak_color_temp > 0){
                  if (_video->_video->raw_settings().temperature == -1){
                     _video->_video->raw_settings().temperature = 5500;
                  }
               }
               else if (raw_changed && tweak_color_temp == 0){
                  _video->_video->raw_settings().temperature = -1;
               }

               if (tweak_color_temp > 0){
                  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x/2);
                  ImGui::SameLine(); raw_changed |= ImGui::SliderInt("Kelvin", &_video->_video->raw_settings().temperature, 2500, 8000, NULL);
               }
               ImGui::EndGroup();

               if (raw_changed){
                  _video->_video->clear_cache();
                  _video->_video->set_dirty();
               }
               ImGui::EndTable();
            }

            raw_changed = false;
            // ACES Table
            ImGui::BeginTable("ACES table", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV);

            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,   0.0f, 0);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,   0.0f, 1);

            ImGui::TableNextColumn();ImGui::Text("Headroom");
            ImGui::TableNextColumn();raw_changed |= ImGui::SliderFloat("##Headroom", &_video->_video->raw_settings().headroom, 1.0f, 6.5f, NULL);

            ImGui::TableNextColumn();ImGui::Text("Blur");
            ImGui::TableNextColumn();raw_changed |= ImGui::SliderFloat("##Blur", &_video->_video->_blur, 1.0f, 20.0f, NULL);


            if (raw_changed){
               _video->_video->set_dirty();
            }

            ImGui::EndTable();

            ImGui::EndTabItem();
         }

         if (ImGui::BeginTabItem("Lens"))
         {
            static bool correct_expo = false;
            static bool correct_distort = false;
            bool lens_expo_changed = false;
            bool lens_distort_changed = false;
            bool lens_changed = false;
 
            ImGui::BeginTable("Lens table", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV);

            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,   0.0f, 0);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,   0.0f, 1);

            ImGui::TableNextColumn();ImGui::Text("Lens");
            ImGui::TableNextColumn();ImGui::Text(_video->_video->lens_name().c_str());
            ImGui::TableNextColumn();ImGui::Text("Focal length");
            ImGui::TableNextColumn();ImGui::Text("%.1f mm", _video->_video->focal_length());
            ImGui::TableNextColumn();ImGui::Text("Focal distance");
            ImGui::TableNextColumn();
            if (_video->_video->focal_dist() < 655.0f){
               ImGui::Text("%.1f m", _video->_video->focal_dist()); 
            } else {
               ImGui::Text("\u221E");
            }
            ImGui::TableNextColumn();ImGui::Text("Aperture");
            ImGui::TableNextColumn();ImGui::Text("f/%.1f", _video->_video->aperture());
            ImGui::EndTable();
            if (ImGui::CollapsingHeader("Lens correction", ImGuiTreeNodeFlags_DefaultOpen)) {
               if (ImGui::Button("Reset to camera")){
                  auto_set_lens();
                  lens_changed = true;
               }
               ImGui::SameLine();ImGui::Checkbox("Use lens id", &_use_lens_id);
               ImGui::BeginTable("Lens correction table", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV);

               ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,   0.0f, 0);
               ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,   0.0f, 1);
               ImGui::TableNextColumn();ImGui::Text("Vignette correction");
               ImGui::TableNextColumn();lens_expo_changed = ImGui::Checkbox("##vignettecorrect", &correct_expo);

               ImGui::TableNextColumn();ImGui::Text("Distortion correction");
               ImGui::TableNextColumn();lens_distort_changed = ImGui::Checkbox("##distortcorrect", &correct_distort);

               ImGui::TableNextColumn();ImGui::Text("Camera model");
               ImGui::TableNextColumn();lens_changed |= ImGui::Combo("##cameramodel", &_sel_camera_model, vector_getter, static_cast<void*>(&_camera_models), _camera_models.size());

               if (lens_changed){
                  update_lenses();
               }
               
               ImGui::TableNextColumn();ImGui::Text("Lens model");
               ImGui::TableNextColumn();lens_changed |= ImGui::Combo("##lensmodel", &_sel_lens_model, vector_getter, static_cast<void*>(&_lens_models), _lens_models.size());

               if (lens_changed){
                  Lens_correction::get_lens_min_max_focal(get_selected_lens(), _min_focal, _max_focal);
                  if (_video->_video->focal_length() < _min_focal || _video->_video->focal_length() > _max_focal){
                     _video->_video->raw_settings().focal_length = _min_focal;
                  }
               }

               ImGui::TableNextColumn();ImGui::Text("Crop factor");
               ImGui::TableNextColumn();lens_changed |= ImGui::SliderFloat("##cropfactor", &_video->_video->raw_settings().crop_factor, 1.0f, 20.0f, NULL);

               if (fabs(_min_focal - _max_focal) > 1.0f){
                  ImGui::TableNextColumn();ImGui::Text("Focal length");
                  ImGui::TableNextColumn();lens_changed |= ImGui::SliderFloat("##focallen", &_video->_video->raw_settings().focal_length, _min_focal, _max_focal, NULL);
               }
               ImGui::EndTable();
            }
            ImGui::EndTabItem();

            if (lens_changed || lens_expo_changed || lens_distort_changed){
               _video->_video->set_lens_params(get_selected_camera(), get_selected_lens(), _video->_video->raw_settings().crop_factor,  _video->_video->aperture(), _video->_video->focal_dist(), _video->_video->raw_settings().focal_length, correct_expo, correct_distort);
            }
         }

         if (ImGui::BeginTabItem("Output"))
         {
            static bool lut_on_gui = false;
            ImGui::BeginTable("Lens table", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV);
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,   0.0f, 0);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,   0.0f, 1);

            ImGui::TableNextColumn();ImGui::Text("Output LUT");
            ImGui::TableNextColumn();
            
            if (_underlying_window->monitor_lut_filename().empty()){
               bool open_lut = ImGui::Button("...");

               if (open_lut){
                  ImGui::OpenPopup("Open File");
               }
               if(_fbw.showFileDialogPopup("Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(600, 300), ".cube")){
                  _underlying_window->set_monitor_lut(_fbw.selected_path);

               }
            }
            ImGui::SameLine();if (!_underlying_window->monitor_lut_filename().empty() && ImGui::Button("X")){
               _underlying_window->set_monitor_lut("");
            }
            
            if (!_underlying_window->monitor_lut_filename().empty()){
               ImGui::SameLine();
               static bool use_lut = true;
               bool changed = ImGui::Checkbox("##use_lut", &use_lut);
               if (changed){
                  _underlying_window->set_lut_texture(use_lut);
               }
            }
            ImGui::SameLine();ImGui::Text(_underlying_window->monitor_lut_filename().c_str());
            if (!_underlying_window->monitor_lut_filename().empty()){
               ImGui::TableNextColumn();ImGui::Text("LUT on GUI");
               ImGui::TableNextColumn();bool lut_on_gui_changed = ImGui::Checkbox("##expocorrect", &lut_on_gui);
               if (lut_on_gui_changed){
                  _underlying_window->set_lut_gui(lut_on_gui);
               }
            }

            ImGui::EndTable();
            ImGui::EndTabItem();
         }
         
         ImGui::EndTabBar();
      }
   }
};

// Main code
int main(int, char**)
{
	App_SDL* app = App_SDL::get();
	std::string fpm_path = app->get_str_config("APP_PATH") + "/../pixel_maps";
	std::string ocio_path = app->get_str_config("APP_PATH") + "/../aces-config/config.ocio";
	setenv("OCIO", ocio_path.c_str(), 1);
	setenv("PIXELMAPSDIR", fpm_path.c_str(), 1);
	Window_SDL* window2 = app->create_new_window("Chrawma", 1200, 900);
   ImGui::GetStyle().FrameRounding = 5.0;
   ImGui::GetStyle().ChildRounding = 5.0;
   ImGui::GetStyle().WindowRounding= 4.0;
   ImGui::GetStyle().GrabRounding = 4.0;
   ImGui::GetStyle().GrabMinSize = 4.0; 
	window2->set_minimum_window_size(800,600);
	TestWidget* testwid = new TestWidget(window2);
   RawInfoWidget* rawinfo = new RawInfoWidget(window2, testwid);
	ImFont* font = app->load_font("/home/cedric/Documents/FONTS/DroidSans.ttf", 16.f);
	app->run();
   return 0;
}
