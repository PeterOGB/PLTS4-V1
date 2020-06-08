/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

// Version 4: Add support for [comments] in editor window.
//            Propper handling of utf8 <->  unicode.
// TODO : Add "Save text" button to witer teleprinter ouput as utf8 file. 



// Version 3: Add Serial / Network comms selection.

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <gtk/gtk.h>
/* For network sockets */
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <math.h>

#include "config.h"
#include "Logging.h"

// Macro for geting widgets from the builder
#define GETWIDGET(name,type) name=type(gtk_builder_get_object_checked (builder,#name))

// Paths to directories. 
static GString *sharedPath = NULL;
static GString *configPath = NULL;

// Widgets etc
GtkWidget *splashWindow;
GtkWidget *splashImage;
static GtkComboBox *connectionCombobox;
GtkWidget *connectionWindow;
GtkAdjustment *ipAdjustments[4];
GtkWidget *mainWindow;

GtkWidget *makingConnectionDialog;
GtkWidget *reconnectButton;
gboolean reconnectButtonState = FALSE;

GtkRecentManager *recentManager;
GtkWidget *openRecentFileChooserDialog;
GtkWidget *loadFileChooserDialog;
GtkWidget *fileUploadButton;
GtkWidget *fileUploadFrameLabel;
GtkWidget *fileTooBigDialog;
GtkWidget *notTelecodeDialog;
GtkWidget *chooseFormatDialog;
GtkWidget *readerEchoButton;
GtkWidget *readerOnlineCheckButton;
GtkWidget *editorUploadButton;
GtkWidget *editorUploadFromCursorButton;
GtkWidget *uploadProgressBar;
GtkWidget *useLocalHost;


GtkWidget *tapeImageDrawingArea;

gboolean   fileUploadWasSensitive = FALSE;
gboolean editorUploadWasSensitive = FALSE;
gboolean editorUploadFromCursorWasSensitive = FALSE;



/**************************** TELEPRINTER ************************/

GByteArray *punchingBuffer = NULL;
guchar runouts[16] = {[0 ... 15] = 0};
guchar EDSACrunouts[16] = {[0 ... 15] = 0x10};

gboolean printing = TRUE;
gboolean punching = FALSE;

gboolean ELLIOTTcode = TRUE;
gboolean MURRYcode = FALSE;

GtkTextView *teleprinterTextView;
GtkTextBuffer *teleprinterTextBuffer;
GtkWidget *windUpFromStartButton;
GtkWidget *windUpFromEndButton;
GtkWidget *discardTapeButton;
GtkWidget *saveFileChooserDialog;
GtkWidget *punchingToTapeButton;

/*--------------------------- READER ----------------------------*/

GtkTextView *readerTextView;
GtkTextBuffer *readerTextBuffer;
gsize fileUploadLength;
gsize fileLength;
gsize fileUploaded;
gchar *fileUploadBuffer;
char *readerFileName = NULL;
gsize *uploadLengthp;
gsize *uploadedp;
const gchar *uploadBuffer;
gboolean readerOnline = FALSE;
int handPosition = 0;
int readerFileType = -1;
int readerEchoType = -1;

/*----------------------------- Editor -----------------------------*/

gsize editorUploadLength;
gsize editorUploaded;
gchar *editorUploadBuffer;
GtkTextBuffer *editorTextBuffer;
GtkTextView *editorTextView;
GtkWidget *editorSaveButton;
GtkWidget *editorFrameLabel;
GtkWidget *editBinaryDialog;
GtkToggleButton *editorELLIOTTmode;
GtkToggleButton *editorEDSACmode;
GString *editorTapeName = NULL;
GtkTextTag *commentTag2 = NULL;
GtkTextTag *commentTag = NULL;
GtkTextTag *errorTag = NULL;

gboolean Backspace = FALSE;
gboolean Delete = FALSE;

GtkWidget *deleteAbortedDialog;

gulong insertHandlerId,deleteHandlerId;
// This was written before I got to grips with Glib's utf8 and unicode
// support features.
/* These tables use strings rather than single characters inorder to easily
   handle the two byte utf-8 code used for £ .
*/
// Elliott telecode
/*
static const gchar  *convert2[] = {NULL,"1","2","*","4","$","=","7","8","'",
			     ",","+",":","-",".","%","0","(",")","3",
			     "?","5","6","/","@","9","£",NULL,NULL,
			     NULL,NULL,NULL,
			     NULL,"A","B","C","D","E","F","G","H", "I"
			     ,"J","K","L","M","N","O","P","Q","R","S",
			     "T","U","V","W","X","Y","Z",NULL," ",
			     "\r","\n",NULL};

*/

// Elliott telecode (done right!)
static const gunichar *ElliottToUnicode =
    U"_""12*4$=78',+:-.%0()3?56/@9£\0\0\0\0\0"
     "_""ABCDEFGHIJKLMNOPQRSTUVWXYZ\0 \r\n\0";

// EDSAC Perforator Code (done right!)
static const gunichar *EDSACToUnicode =
     U"0123456789\0\0\0\0\0\0\0\0\0\0\0+-\0\0\0\0\0\0\0\0\0"
      "PQWERTYUIOJ#SZK*.F@D!HNM&LXGABCV";

#if 0
// Murry Code
static const  gchar *convert3[] = {NULL,"3",NULL,"-"," ","'","8","7",NULL,NULL,"4",NULL,
			      ",","!",":","(","5","+",")","2","$","6","0","1","9","?","&",
			      NULL,".","/","=",NULL,
			      NULL,"E",NULL,"A"," ","S","I","U",NULL,"D","R","J","N",
			      "F","C","K","T","Z","L","W","H","Y","P","Q","O","B","G",
			      NULL,"M","X","V",NULL};

static const gunichar *MURRYToUnicode =
    U"\0""3\0- '87""\0\0""4\0,!:(5+)2$6019?&\0./=\0"
     "\0E\0A SIU\0DRJNFCKTZLWHYPQOBG\0MXV\0";
#endif

#if 0
// EDSAC Perforator code
static const  gchar *convert4[] = {"0","1","2","3","4","5","6","7",
				   "8","9",NULL,NULL,NULL,NULL,NULL,NULL,
				   NULL,NULL,NULL,NULL,NULL,"+","-",NULL,
				   NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
				   "P","Q","W","E","R","T","Y","U",
				   "I","O","J","#","S","Z","K","*",
				   ".","F","@","D","!","H","N","M",
				   "&","L","X","G","A","B","C","V"};
#endif
static GIOChannel *E803_channel = NULL;
static guint RxWatchId=0,TxWatchId=0,ErWatchId=0;

static gchar *BOMs[];
//static const gchar *BOMnames[];

/******************* Prototypes for GTK event handlers **************************/

static GObject *gtk_builder_get_object_checked(GtkBuilder *builder,const gchar *name);
gboolean on_quitButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data);
gboolean on_serialConnectButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused)) gpointer data);
gboolean on_networkConnectButton_clicked(__attribute__((unused)) GtkButton *button,
					 __attribute__((unused)) gpointer data);
gboolean on_noConnectionButton_clicked(__attribute__((unused)) GtkButton *widget,
				       __attribute__((unused)) gpointer data);
gboolean on_mainQuitButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data);
gboolean on_reconnectButton_clicked(__attribute__((unused)) GtkButton *widget,
				    __attribute__((unused)) gpointer data);
gboolean
on_fileUploadSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
				     __attribute__((unused))gpointer data);
gboolean
on_fileUploadButton_clicked(__attribute__((unused)) GtkButton *button,
			      __attribute__((unused)) gpointer data);

static gboolean
E803_messageHandler(GIOChannel *source,
		    __attribute__((unused)) GIOCondition condition,
		    __attribute__((unused)) gpointer data);

gboolean
on_printToScreenButton_toggled( GtkToggleButton *button,
				__attribute__((unused))gpointer data);

gboolean
on_punchingToTapeButton_toggled(GtkToggleButton *button,
				__attribute__((unused)) gpointer data);

gboolean
on_windUpFromEndButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))	gpointer data);

gboolean
on_windUpFromStartButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data);

gboolean
on_discardTapeButton_clicked(__attribute__((unused)) GtkButton *button,
			     __attribute__((unused))	gpointer data);
gboolean
on_clearTeleprinterTextButton_clicked(__attribute__((unused)) GtkButton *button,
				      __attribute__((unused))	gpointer data);

gboolean
on_readerEchoButton_toggled(GtkWidget *button,
			    __attribute__((unused))gpointer user_data);

gboolean
on_fileUploadChooseRecentFileButton_clicked(__attribute__((unused)) GtkButton *button,
					      __attribute__((unused))gpointer data);

gboolean on_editorTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
					   GdkEventKey *event);

/*
void
on_editorTextView_insert_at_cursor(GtkTextView *text_view,
				   gchar       *string,
				   gpointer     user_data);
*/
void
on_editorTextBuffer_delete_range(GtkTextBuffer *textbuffer,
				 GtkTextIter   *start,
				 GtkTextIter   *end,
				 __attribute__((unused))gpointer user_data);

void on_editorTextBuffer_insert_text(GtkTextBuffer *textbuffer,
				     GtkTextIter   *location,
				     gchar         *text,
				     gint           len,
				     gpointer       user_data);

void on_editorTextBuffer_changed(GtkTextBuffer *textbuffer,
				 gpointer       user_data);
gboolean
on_editorNewButton_clicked(__attribute__((unused)) GtkButton *button,
			   __attribute__((unused))gpointer data);

gboolean
on_editorOldButton_clicked(__attribute__((unused)) GtkButton *widget,
			   __attribute__((unused))gpointer data);

gboolean
on_editorUploadButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data);

gboolean
on_editorUploadFromCursorButton_clicked(__attribute__((unused)) GtkButton *button,
					  __attribute__((unused)) gpointer data);

gboolean
on_editorSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))	gpointer data);

gboolean
on_editorSaveButton_clicked(__attribute__((unused)) GtkButton *button,
			    __attribute__((unused))gpointer data);
gboolean
on_commentTestButton_clicked(__attribute__((unused)) GtkButton *button,
			    __attribute__((unused))gpointer data);


gboolean
on_readerOnlineCheckButton_toggled(__attribute__((unused)) GtkButton *button,
				   __attribute__((unused)) gpointer data);
gboolean
on_readerTextView_key_press_eventt(__attribute__((unused))GtkWidget *widget,
				   GdkEventKey *event);

gboolean
on_readerTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
				  GdkEventKey *event);

gboolean
on_useMurryCodeButton_toggled(__attribute__((unused)) GtkButton *button,
			      __attribute__((unused)) gpointer data);

gboolean
on_clearReaderTextButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data);

gboolean
on_setDefaultAddressButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data);


gboolean
on_tapeImageDrawingArea_configure_event(__attribute__((unused)) GtkWidget *widget,
					GdkEventConfigure  *event,
					__attribute__((unused)) gpointer   user_data);

gboolean
on_tapeImageDrawingArea_draw(GtkWidget *da,
			     cairo_t *cr,
			     __attribute__((unused))gpointer udata);



gboolean
mouseMotionWhilePressed (__attribute__((unused)) GtkWidget      *tape,
			 __attribute__((unused)) GdkEventMotion *event,
			 __attribute__((unused)) gpointer        data);

gboolean
on_tapeImageDrawingArea_button_press_event(__attribute__((unused))GtkWidget *tape,
					   __attribute__((unused))GdkEventButton *event,
					   __attribute__((unused))gpointer data);

gboolean
on_tapeImageDrawingArea_button_release_event(__attribute__((unused))GtkWidget *tape,
					     __attribute__((unused))GdkEventButton *event,
					     __attribute__((unused))gpointer data);


gboolean
on_editorELLIOTTmode_toggled(__attribute__((unused))GtkWidget *tape,
				    __attribute__((unused))GdkEventButton *event,
				    __attribute__((unused))gpointer data);


gboolean
on_editorEDSACmode_toggled(__attribute__((unused))GtkWidget *tape,
				    __attribute__((unused))GdkEventButton *event,
				    __attribute__((unused))gpointer data);


// Also called directly from F1 and F2 oresses
gboolean on_commentUnwrapButton_clicked(__attribute__((unused)) GtkButton *widget,
					__attribute__((unused)) gpointer data);

gboolean on_commentWrapButton_clicked(__attribute__((unused)) GtkButton *widget,
				      __attribute__((unused)) gpointer data);

static int commentDepthAtIter2(GtkTextIter *location);

static void stripRunouts(GByteArray *telecode,guint8 runoutCh);
    
static gboolean
notTelecode(GdkEventKey *event,gboolean Online);

/* Widget Event Handlers */
/* Connection window */

gboolean
on_quitButton_clicked(__attribute__((unused)) GtkButton *widget,
		      __attribute__((unused)) gpointer data)
{
    gtk_main_quit();
    return GDK_EVENT_STOP ;
}
gboolean
on_noConnectionButton_clicked(__attribute__((unused)) GtkButton *widget,
		      __attribute__((unused)) gpointer data)
{

    if(!gtk_widget_get_visible(mainWindow))
    {
	//title = g_string_new(NULL);
	//g_string_printf(title,"Connected to %s",address->str);
	//gtk_window_set_title(GTK_WINDOW(mainWindow),title->str);
	gtk_widget_show(mainWindow);
	    
    }
    gtk_widget_hide(connectionWindow);
    return GDK_EVENT_PROPAGATE ;
}

// Get address from spinners and save to file
gboolean
on_setDefaultAddressButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data)
{
    GString *address;
    	GString *configFileName;
	GIOChannel *file;
	GError *error = NULL;

    address = g_string_new(NULL);
    g_string_printf(address,"%d.%d.%d.%d\n",
			(int)gtk_adjustment_get_value(ipAdjustments[0]),
			(int)gtk_adjustment_get_value(ipAdjustments[1]),
			(int)gtk_adjustment_get_value(ipAdjustments[2]),
			(int)gtk_adjustment_get_value(ipAdjustments[3]));


    g_info("Adddress set to (%s)\n",address->str);
    configFileName = g_string_new(configPath->str);
    g_string_append(configFileName,"DefaultIP");

    if((file = g_io_channel_new_file(configFileName->str,"w",&error)) == NULL)
    {
	g_warning("failed to open file %s due to %s\n",configFileName->str,error->message);
    }
    else
    {
	g_io_channel_write_chars(file,address->str,-1,NULL,NULL);
	g_io_channel_shutdown(file,TRUE,NULL);
	g_io_channel_unref (file);
    }
    
    return GDK_EVENT_PROPAGATE ;
}
    


gboolean
on_serialConnectButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused)) gpointer data)
{
    //gint active;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchararray name;
    GString *title;
    
    struct termios newtio;
    int serial_fd;

    // Get selected device fron combobox
    //active = gtk_combo_box_get_active(connectionCombobox);
    model = gtk_combo_box_get_model(connectionCombobox);
   
    gtk_combo_box_get_active_iter(connectionCombobox,&iter);

    gtk_tree_model_get(model,&iter,0,&name,-1);

    serial_fd = open(name,O_RDWR|O_NONBLOCK);

    tcgetattr(serial_fd,&newtio);
    cfsetospeed(&newtio,B57600);
    newtio.c_cc[VMIN] = 0;
    newtio.c_cc[VTIME] = 0;
    cfmakeraw(&newtio);
    newtio.c_cflag &= ~CRTSCTS;
    tcsetattr(serial_fd,TCSANOW,&newtio);

    E803_channel = g_io_channel_unix_new(serial_fd);
  
    RxWatchId = g_io_add_watch(E803_channel,G_IO_IN ,E803_messageHandler,NULL);

    g_io_channel_set_encoding(E803_channel,NULL,NULL);
    // Channel needs to be unbuffered otherwise interleaved reads and writes lead to
    // "Illegal seek" errors !
    g_io_channel_set_buffered(E803_channel,FALSE);

    gtk_widget_hide(connectionWindow);
    if(!gtk_widget_get_visible(mainWindow))
    {
	gtk_widget_show(mainWindow);
    }
    title = g_string_new(NULL);
    g_string_printf(title,"Connected to PLTS via %s",name);
    gtk_window_set_title(GTK_WINDOW(mainWindow),title->str);
    g_string_free(title,TRUE);
				 
    gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-disconnect");
    reconnectButtonState = TRUE;
     
    return GDK_EVENT_PROPAGATE ;
}


/* Main window event handlers */

gboolean on_mainQuitButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data)
{
    gtk_main_quit();
    return TRUE;
}

gboolean on_reconnectButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data)
{
    if(reconnectButtonState)
    {
	g_source_remove(RxWatchId);
	if(ErWatchId>0)	g_source_remove(ErWatchId);
	g_io_channel_shutdown(E803_channel,FALSE,NULL);
	g_io_channel_unref(E803_channel);
	E803_channel = NULL;
	gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-connect");
	reconnectButtonState = FALSE;
	gtk_window_set_title(GTK_WINDOW(mainWindow),"Disconnected");
	gtk_widget_set_sensitive(editorUploadButton,FALSE);
	gtk_widget_set_sensitive(editorUploadFromCursorButton,FALSE);
	gtk_widget_set_sensitive(fileUploadButton,FALSE);
    }
    else
    {
	
	gtk_widget_show (connectionWindow);
    }
    return GDK_EVENT_PROPAGATE ;
}


/*
Count numbers of CR , LF and CRLF pairs
Use threshold on the variace as discriminator between text and binary files
 */

static gboolean
isBinaryTape(gchar *buf,gsize len)
{
    gsize count;
    gint crCount,lfCount,crlfCount;
    //gint diff1;
    gchar this,prev;
    const gint8 *cp;
    gfloat mean,variance;

    cp = (gint8 *) buf;;
    
    count = len;
    crCount = lfCount = crlfCount = 0;

    this = prev = 0x00;
    while(count-- > 1)
    {
	prev = this;
	this = *cp++ & 0x1F;
	
	// Check for CR/LF sequences
	if(this == 0x1D) crCount+= 1;
	if(this == 0x1E) lfCount+= 1;

	if( (this == 0x1E) &&
	    (prev == 0x1D) ) crlfCount+= 1;
    }

   

    mean = ((float)(crCount + lfCount + crlfCount)) / 3.0f; 

    variance  = powf((float)crCount  -  mean,2.0f);
    variance += powf((float)lfCount  -  mean,2.0f);
    variance += powf((float)crlfCount - mean,2.0f);

    variance /= 2.0f;

    //printf("%s cr=%d lf=%d crlf=%d variance = %f\n",__FUNCTION__,
//	   crCount,lfCount,crlfCount,(double) variance);


    if(variance > 200.0f)
	return TRUE;
    else
	return FALSE;


}



static gchar *EDSACtoUft8(gchar *dataBuffer,gsize length,gsize *bytesWritten)
{
    GByteArray *utf8Buffer;
    gchar *results;
    gsize index;
    gunichar txt;
    gchar utf8Ch[6];
    gint utf8ChLength,ch,width;
    gunichar cr = U'\r';
    
    utf8Buffer = g_byte_array_new();
    width = 0;
    
    for(index=0;index<length;index++)
    {
	txt = 0;
	ch = dataBuffer[index] & 0x3F;

	//printf(">>> 0x%02x\n",ch);

	txt = EDSACToUnicode[ch];

	if(txt != 0)
	{
	    utf8ChLength = g_unichar_to_utf8 (txt,utf8Ch);
	    g_byte_array_append(utf8Buffer,(guchar *) utf8Ch,(guint) utf8ChLength);
	    width += 1;
	    if(width == 32)
	    {
		width = 0;
		utf8ChLength = g_unichar_to_utf8 (cr,utf8Ch);
		g_byte_array_append(utf8Buffer,(guchar *) utf8Ch,(guint) utf8ChLength);
	    }
	}
    }
    
    utf8Ch[0] = 0;
    g_byte_array_append(utf8Buffer,(guchar *) utf8Ch,1);
    
    *bytesWritten = utf8Buffer->len;
    results = (gchar *) g_byte_array_free(utf8Buffer,FALSE);
    return results;
}

static gchar *ElliottToUtf8(gchar *dataBuffer,gsize length,gsize *bytesWritten)
{
    GByteArray *utf8Buffer;
    gchar *results;
    gboolean letters,figures;
    gsize index;
    gunichar txt;
    gchar utf8Ch[6];
    gint utf8ChLength,ch;
    
    utf8Buffer = g_byte_array_new();
    
    letters = figures = FALSE;

    for(index=0;index<length;index++)
    {
	txt = 0;
	ch = dataBuffer[index] & 0x1F;
	if(ch >= 0x1B)
	{
	    switch(ch)
	    {
	    case 0x1B:
		figures = TRUE;
		letters = FALSE;
		txt = 0;
		break;
	    case 0x1C:
		txt = U' ';
		break;
	    case 0x1D:
		txt = U'\n';
		break;
	    case 0x1E:
		break;
	    case 0x1F:
		figures = FALSE;
		letters = TRUE;
		break;
	    }
	}
	else
	{
	    // Removed this as it now mangles leading runouts.
	    //if( letters || figures)
	    {
		if(letters) ch += (gchar) 32;
		txt = ElliottToUnicode[ch];
	    }
	}

	if(txt != 0)
	{
	    utf8ChLength = g_unichar_to_utf8 (txt,utf8Ch);
	    g_byte_array_append(utf8Buffer,(guchar *) utf8Ch,(guint) utf8ChLength);
	}
    }

    utf8Ch[0] = 0;
    g_byte_array_append(utf8Buffer,(guchar *) utf8Ch,1);
	
    
    *bytesWritten = utf8Buffer->len;
    results = (gchar *) g_byte_array_free(utf8Buffer,FALSE);
    return results;
}



/*************** READER ********************/

gboolean
on_fileUploadSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
				     __attribute__((unused))gpointer data)
{
    //GtkWidget *dialog;
    gint res;
    gboolean BOMmatch;
    gint type; 

    gtk_widget_set_sensitive(fileUploadButton,FALSE);
        
    res = gtk_dialog_run (GTK_DIALOG (loadFileChooserDialog));
    gtk_widget_hide(loadFileChooserDialog);

    if (res == GTK_RESPONSE_OK)
    {
	GString *title;
	
	if(readerFileName != NULL)
	    g_free (readerFileName);
	readerFileName = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (loadFileChooserDialog));

	title = g_string_new("File Upload: ");
	g_string_append(title,readerFileName);
	gtk_label_set_text(GTK_LABEL(fileUploadFrameLabel),title->str);
	g_string_free(title,TRUE);
    }
    else
    {
	if(fileUploadBuffer != NULL)
	    gtk_widget_set_sensitive(fileUploadButton,TRUE);
	return GDK_EVENT_PROPAGATE ;
    }
    
    if(fileUploadBuffer != NULL)
	g_free(fileUploadBuffer);

    {
	GFile *gf;
	GError *error = NULL;
	
	gf = g_file_new_for_path(readerFileName);
	
	g_file_load_contents (gf,NULL,&fileUploadBuffer,
			      &fileLength,NULL,&error);
	//mask6holes(fileUploadBuffer,fileLength);
	g_object_unref(gf);
    }

    readerFileType = -1;
    BOMmatch = FALSE;
    
    for(type = 3; type < 7; type++)
    {
	if(strncmp(fileUploadBuffer,BOMs[type],type < 2 ? 2 : 3 ) == 0)
	{
	    BOMmatch = TRUE;
	    break;
	}
    }

    if(BOMmatch)
    {
	//printf("upload is type %s\n",BOMnames[type]);
	// Turn off echo for ELLIOTT binary tapes
	if(((type == 3) || (type == 4) ) && isBinaryTape(fileUploadBuffer,fileLength))
	{
	    g_info("BINARY TAPE,echo off\n");
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(readerEchoButton),FALSE);
	}
	if((type == 5) || (type == 6))
	{
	    gsize count;
	    gchar *cp;

	    // Strip leading runouts.
	    count = 0;
	    cp = fileUploadBuffer;

	    while(((*cp == '\x10' ) || (*cp == '\x90' )) && (count != fileLength))
	    {
		cp++;
		count++;
	    }

	    if(count != 0)
	    {
		memmove(fileUploadBuffer,cp, fileLength-count);
		fileLength -= count;
	    }

	}
	readerFileType = type;
    }
    else
    {
	//printf("upload is not ELLIOTT or EDSAC!\n");
	res = gtk_dialog_run (GTK_DIALOG (notTelecodeDialog));
	gtk_widget_hide(notTelecodeDialog);

	g_free(fileUploadBuffer);
	fileUploadBuffer = NULL;
	
	return GDK_EVENT_PROPAGATE ;
    }
    


    
    if(fileLength > 65536)
    {
	res = gtk_dialog_run (GTK_DIALOG (fileTooBigDialog));
	gtk_widget_hide(fileTooBigDialog);
	g_free(fileUploadBuffer);
	fileUploadBuffer = NULL;
	return GDK_EVENT_PROPAGATE ;
    }

    gtk_widget_set_sensitive(fileUploadButton,TRUE);
#if 0
    {
	gsize n;
	char *cp;
	cp = fileUploadBuffer;
	n = fileLength;
	while(n--) *cp++ |= '\x80';
    }
#endif
    // Update the tape image
    gtk_widget_queue_draw(tapeImageDrawingArea);

    return GDK_EVENT_PROPAGATE ;
}


// Turn echoing from the PLTS on and off
gboolean
on_readerEchoButton_toggled(GtkWidget *button,
			    __attribute__((unused))gpointer user_data)
{
  gchar value[1];
  
  gboolean set;
  gsize written;
  GError *error = NULL;

  //printf("%s called\n",__FUNCTION__);

  set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

  if(set) 
     value[0] = '\x84';
  else
    value[0] = '\x85';

  g_io_channel_write_chars(E803_channel,value,1,&written,&error);
  g_io_channel_flush(E803_channel,NULL);

  return GDK_EVENT_PROPAGATE ;
}


gboolean
on_fileUploadButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data)
{
    gchar value[3];
    GError *error = NULL;
    gsize written;

    // Buffer contents have already been validated in
    // on_fileUploadSetFileButton_clicked and
    // on_fileUploadChooseRecentFileButton_clicked

    // update readerEchoType to match latest selected file
    readerEchoType = readerFileType;
    
    value[0] = '\x80';
    value[1] = '\x00';
    value[2] = '\x00';
    
    g_io_channel_write_chars(E803_channel,value,3,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    // The cast to intmax_t makes this work as expected on 
    // the 32 bit ARM CPU inthe Pi. 
    //printf("file size = %jd \n",(__intmax_t)fileLength);
    fileUploaded = 0;

    // Don't modify the fileLength as it may get uploaded again !  (bug fix) 
    fileUploadLength = fileLength - ((gsize)handPosition/8);
    uploadLengthp = &fileUploadLength;
    uploadedp = &fileUploaded;
    uploadBuffer = &fileUploadBuffer[handPosition/8];

    // Save button states
    fileUploadWasSensitive   = gtk_widget_get_sensitive(fileUploadButton);
    editorUploadWasSensitive = gtk_widget_get_sensitive(editorUploadButton);
    editorUploadFromCursorWasSensitive = gtk_widget_get_sensitive(editorUploadFromCursorButton);
    
    gtk_widget_set_sensitive(fileUploadButton,FALSE);
    gtk_widget_set_sensitive(editorUploadButton,FALSE);
    gtk_widget_set_sensitive(editorUploadFromCursorButton,FALSE);
    return GDK_EVENT_PROPAGATE ;
}


gboolean
on_fileUploadChooseRecentFileButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))gpointer data)
{
    gint res;
    gboolean BOMmatch;
    gint type; 
    

    res = gtk_dialog_run (GTK_DIALOG (openRecentFileChooserDialog));
    gtk_widget_hide(openRecentFileChooserDialog);

    if (res == GTK_RESPONSE_OK)
    {
	GString *title;
	GtkRecentInfo *info;
	
	if(readerFileName != NULL)
	    g_free (readerFileName);

	info =  gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER(openRecentFileChooserDialog));
	readerFileName = gtk_recent_info_get_uri_display(info);

	
	title = g_string_new("File Upload: ");
	g_string_append(title,readerFileName);
	gtk_label_set_text(GTK_LABEL(fileUploadFrameLabel),title->str);
	g_string_free(title,TRUE);
	gtk_recent_info_unref (info);
    }
    else
    {
	if(fileUploadBuffer != NULL)
	    gtk_widget_set_sensitive(fileUploadButton,TRUE);
	return GDK_EVENT_STOP;
    }
    
    if(fileUploadBuffer != NULL)
	g_free(fileUploadBuffer);


    {
	GFile *gf;
	GError *error = NULL;
	
	gf = g_file_new_for_path(readerFileName);

	g_file_load_contents (gf,NULL,&fileUploadBuffer,
			      &fileLength,NULL,&error);
	//mask5holes(fileUploadBuffer,fileLength);
	
	//TODO Check error returned

	g_object_unref(gf);
    }

    readerFileType = -1;
    BOMmatch = FALSE;
    
    for(type = 3; type < 7; type++)
    {
	if(strncmp(fileUploadBuffer,BOMs[type],type < 2 ? 2 : 3 ) == 0)
	{
	    BOMmatch = TRUE;
	    break;
	}
    }

    if(BOMmatch)
    {
	//printf("upload is type %s\n",BOMnames[type]);
	// Turn off echo for ELLIOTT binary tapes
	if(((type == 3) || (type == 4) ) && isBinaryTape(fileUploadBuffer,fileLength))
	{
	    g_info("BINARY TAPE,echo off\n");
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(readerEchoButton),FALSE);
	}
	readerFileType = type;
    }
    else
    {
	//printf("upload is not ELLIOTT or EDSAC!\n");
	res = gtk_dialog_run (GTK_DIALOG (notTelecodeDialog));
	gtk_widget_hide(notTelecodeDialog);

	g_free(fileUploadBuffer);
	fileUploadBuffer = NULL;
	
	return GDK_EVENT_PROPAGATE ;
    }
    
    if(fileLength > 65536)
    {
	res = gtk_dialog_run (GTK_DIALOG (fileTooBigDialog));
	gtk_widget_hide(fileTooBigDialog);
	g_free(fileUploadBuffer);
	fileUploadBuffer = NULL;
	return GDK_EVENT_STOP ;
    }

    gtk_widget_set_sensitive(fileUploadButton,TRUE);

    // Update the tape image
    gtk_widget_queue_draw(tapeImageDrawingArea);
    return GDK_EVENT_PROPAGATE ;
    
}

static gboolean goingOnline = FALSE;

gboolean
on_readerOnlineCheckButton_toggled(__attribute__((unused)) GtkButton *button,
				   __attribute__((unused)) gpointer data)
{
    gchar value[1];
    gsize written;
    GError *error = NULL;
      
    readerOnline = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ;

    if(readerOnline)
    {
	value[0] = '\x88';
	goingOnline = TRUE;
    }
    else
	value[0] = '\x89';

    g_io_channel_write_chars(E803_channel,value,1,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    return GDK_EVENT_PROPAGATE;
}

guint keyToTelecode[32];
guint keyToMurryCode[64];

gboolean
on_readerTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
				   GdkEventKey *event)
{
    gboolean ignore;
       gchar value[2];
    guint telecode,n;
 
    gsize written;
    GError *error = NULL;
    static gboolean forceShift = FALSE;
    
    if(readerOnline)
    {
	ignore = notTelecode(event,TRUE);
	if(ignore) return GDK_EVENT_STOP;

	if(goingOnline)
	{
	    forceShift = TRUE;
	    goingOnline = FALSE;
	}

	if(ELLIOTTcode)
	{
	    if((event->keyval >= GDK_KEY_A) && (event->keyval <= GDK_KEY_Z))
	    {
		telecode = 1 + event->keyval - GDK_KEY_A ;
		telecode += 32;
	    }
	    else if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
	    {
		telecode = 1+ event->keyval - GDK_KEY_a;
		telecode += 32;
	    }
	    else
	    {
		for(n=0;n<32;n++)
		    if(keyToTelecode[n] == event->keyval)
		    {
			//if(n == 29) n = 30;   // CR -> LF
			// Always send a FS or LS after a CR
			if(n == 29) forceShift = TRUE;
			telecode = n;
			if(n >= 27) telecode += 64;
		    }
	    }
	}

	if(MURRYcode)
	{
	    for(n=0;n<64;n++)
	    {
		if(keyToMurryCode[n] == event->keyval)
		{
		    if(n == 2) forceShift = TRUE;
		    telecode = n;
		    if((n == 4)||(n == 8)||(n == 2)) telecode += 64;
		}
	    }
	
	}
	if(forceShift && (telecode < 64))
	{
	    telecode += 0x80;
	    forceShift = FALSE;
	}
	    
       	value[0] = '\x8A';
	value[1] = (gchar) (telecode & 0xFF);

	g_io_channel_write_chars(E803_channel,value,2,&written,&error);
	g_io_channel_flush(E803_channel,NULL);

	return GDK_EVENT_STOP;
    }
    else
    {
	return GDK_EVENT_STOP;
    }
}

gboolean
on_clearReaderTextButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data)
{
    GtkTextIter start,end;

    gtk_text_buffer_get_start_iter (readerTextBuffer,&start);
    gtk_text_buffer_get_end_iter (readerTextBuffer,&end);

    gtk_text_buffer_delete (readerTextBuffer,&start,&end);
    return GDK_EVENT_PROPAGATE;
}

/************************** TELEPRINTER *************************/



gboolean
on_printToScreenButton_toggled( GtkToggleButton *button,
				__attribute__((unused))gpointer data)
{
  gboolean set;

  set = gtk_toggle_button_get_active(button);
  if(set) 
     printing = TRUE;
  else
    printing = FALSE;

  return GDK_EVENT_PROPAGATE ;
}

gboolean
on_punchingToTapeButton_toggled(GtkToggleButton *button,
			     __attribute__((unused)) gpointer data)
{
  gboolean set;

  set = gtk_toggle_button_get_active(button);
  if(set)
  {
      punching = TRUE;
      if(punchingBuffer == NULL)
      {
	  punchingBuffer = g_byte_array_sized_new(1024);
	  g_byte_array_append(punchingBuffer,runouts,16);
      }
      
      gtk_widget_set_sensitive(windUpFromStartButton,TRUE);
      gtk_widget_set_sensitive(windUpFromEndButton,TRUE);
      gtk_widget_set_sensitive(discardTapeButton,TRUE);
      
  }  
  else
      punching = FALSE;

  return GDK_EVENT_PROPAGATE ;
}

static gboolean
saveBuffer(GByteArray *buffer)
{
    gint res;

    res = gtk_dialog_run (GTK_DIALOG (saveFileChooserDialog));

    gtk_widget_hide(saveFileChooserDialog);
    
    if (res == GTK_RESPONSE_OK)
    {
	gchar *punchFileName;
	punchFileName = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (saveFileChooserDialog));
	{
	    GFile *gf;
	    GError *error = NULL;
	    gf = g_file_new_for_path(punchFileName);

	    g_file_replace_contents (gf,(char *) buffer->data, buffer->len,
				     NULL,FALSE,G_FILE_CREATE_NONE,
				     NULL,NULL,&error);
	    g_object_unref(gf);
	}	
	
	g_free(punchFileName);
	return TRUE;
    }

    return FALSE;
}


gboolean 
on_windUpFromEndButton_clicked(__attribute__((unused)) GtkButton *button,
			     __attribute__((unused))	gpointer data)
{
    g_byte_array_append(punchingBuffer,runouts,16);
    if(saveBuffer(punchingBuffer))
    {
	g_byte_array_free(punchingBuffer,TRUE);
	punchingBuffer = NULL;

	gtk_widget_set_sensitive(windUpFromStartButton,FALSE);
	gtk_widget_set_sensitive(windUpFromEndButton,FALSE);
	gtk_widget_set_sensitive(discardTapeButton,FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(punchingToTapeButton),FALSE);
    }
    return  GDK_EVENT_PROPAGATE;
}


static void
reverseBuffer(GByteArray *buffer)
{
    guint8 *data,tmp;
    guint fromStart,fromEnd; 

    data = buffer->data;
    // BUg fix 29/9/19   Needed -1
    fromEnd = buffer->len - 1;

    for(fromStart = 0; fromStart < fromEnd; fromStart ++, fromEnd --) {
	tmp = data[fromStart];
	data[fromStart] = data[fromEnd];
	data[fromEnd] = tmp;
    }
}


gboolean
on_windUpFromStartButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data)
{
    // Add runouts to the end that will become blank header
    g_byte_array_append(punchingBuffer,runouts,16);
    reverseBuffer(punchingBuffer);
    
    if(saveBuffer(punchingBuffer))
    {
	g_byte_array_free(punchingBuffer,TRUE);
	punchingBuffer = NULL;

	gtk_widget_set_sensitive(windUpFromStartButton,FALSE);
	gtk_widget_set_sensitive(windUpFromEndButton,FALSE);
	gtk_widget_set_sensitive(discardTapeButton,FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(punchingToTapeButton),FALSE);
    }
    else
    {
	// Restore if not saved
	reverseBuffer(punchingBuffer);
    }

    return  GDK_EVENT_PROPAGATE;
}

gboolean
on_discardTapeButton_clicked(__attribute__((unused)) GtkButton *button,
			     __attribute__((unused))	gpointer data)
{
    //printf("%s called\n",__FUNCTION__);
    if(punchingBuffer != NULL)
	g_byte_array_set_size(punchingBuffer,16);  // Leave runouts in place

    if(!punching)
    {
	gtk_widget_set_sensitive(windUpFromStartButton,FALSE);
	gtk_widget_set_sensitive(windUpFromEndButton,FALSE);
	gtk_widget_set_sensitive(discardTapeButton,FALSE);
    }
    
    return  GDK_EVENT_PROPAGATE;
}


gboolean
on_clearTeleprinterTextButton_clicked(__attribute__((unused)) GtkButton *button,
				      __attribute__((unused))	gpointer data)
{
    GtkTextIter start,end;

    gtk_text_buffer_get_start_iter (teleprinterTextBuffer,&start);
    gtk_text_buffer_get_end_iter (teleprinterTextBuffer,&end);

    gtk_text_buffer_delete (teleprinterTextBuffer,&start,&end);

    return  GDK_EVENT_PROPAGATE;
}

gboolean
on_useMurryCodeButton_toggled(__attribute__((unused)) GtkButton *button,
			      __attribute__((unused)) gpointer data)
{
    gboolean set;
      
    set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ;
    //printf("%s %s\n",__FUNCTION__,set ? "TRUE" : "FALSE");

    if(set)
    {
	MURRYcode = TRUE;
	ELLIOTTcode = FALSE;
    }
    else
    {
	MURRYcode = FALSE;
	ELLIOTTcode = TRUE;
    }	
    return  GDK_EVENT_PROPAGATE;
}


/*********************************** EDITOR ********************************/


/* GDK_KEY codes for the non-letter and non-digit characters in Elliott Telecode */
guint telecodeKeysElliott[16] = {GDK_KEY_asterisk,GDK_KEY_dollar,GDK_KEY_equal,GDK_KEY_apostrophe,
			GDK_KEY_comma,GDK_KEY_plus,GDK_KEY_colon,GDK_KEY_minus,
			GDK_KEY_period,GDK_KEY_percent,GDK_KEY_parenleft,
			GDK_KEY_parenright,GDK_KEY_question,GDK_KEY_slash,GDK_KEY_at,
			GDK_KEY_sterling};

/* GDK_KEY codes for the non-letter and non-digit characters in EDSAC Perforator Telecode */
guint telecodeKeysEDSAC[8] = {GDK_KEY_asterisk,GDK_KEY_plus,GDK_KEY_minus,
			       GDK_KEY_period,GDK_KEY_at,GDK_KEY_numbersign,
			       GDK_KEY_exclam,GDK_KEY_ampersand};


/* GDK_KEY codes for the non-letter and non-digit characters in Murry code */
guint telecodeKeysMurry[14] = {GDK_KEY_minus,GDK_KEY_question,GDK_KEY_colon,
				 GDK_KEY_percent,GDK_KEY_at,GDK_KEY_sterling,
				 GDK_KEY_parenleft,GDK_KEY_parenright,GDK_KEY_period,
				 GDK_KEY_comma,GDK_KEY_apostrophe,GDK_KEY_equal,
				 GDK_KEY_slash,GDK_KEY_plus};



/* GDK_KEY codes for other allowed key presses */
guint cursorKeys[7] = {GDK_KEY_Return,GDK_KEY_BackSpace,GDK_KEY_Left,GDK_KEY_Up,
		       GDK_KEY_Right,GDK_KEY_Down,GDK_KEY_space };



// Used in "Reader Online" mode to convert keypresses into telecode values
// Only used for figures as letters are logical !
guint keyToTelecode[32] = {0,GDK_KEY_1,GDK_KEY_2,GDK_KEY_asterisk,GDK_KEY_4,
			   GDK_KEY_dollar,GDK_KEY_equal,GDK_KEY_7,GDK_KEY_8,
			   GDK_KEY_apostrophe,GDK_KEY_comma,GDK_KEY_plus,
			   GDK_KEY_colon,GDK_KEY_minus,GDK_KEY_period,
			   GDK_KEY_percent,GDK_KEY_0,GDK_KEY_parenleft,
			   GDK_KEY_parenright,GDK_KEY_3,GDK_KEY_question,
			   GDK_KEY_5,GDK_KEY_6,GDK_KEY_slash,GDK_KEY_at,
			   GDK_KEY_9,GDK_KEY_sterling,0,GDK_KEY_space,
			   GDK_KEY_Return,GDK_KEY_BackSpace,0};

guint keyToMurryCode[64] = {0,GDK_KEY_5,GDK_KEY_Return,GDK_KEY_9,GDK_KEY_space,GDK_KEY_sterling,
			    GDK_KEY_comma,GDK_KEY_period,GDK_KEY_BackSpace,GDK_KEY_parenright,
			    GDK_KEY_4,GDK_KEY_at,GDK_KEY_8,GDK_KEY_0,GDK_KEY_colon,GDK_KEY_equal,
			    GDK_KEY_3,GDK_KEY_plus,0,GDK_KEY_question,GDK_KEY_apostrophe,
			    GDK_KEY_6,GDK_KEY_percent,GDK_KEY_slash,GDK_KEY_minus,GDK_KEY_2,0,
			    0,GDK_KEY_7,GDK_KEY_1,GDK_KEY_parenleft,0,

			    0,GDK_KEY_t,GDK_KEY_Return,GDK_KEY_o,GDK_KEY_space,GDK_KEY_h,GDK_KEY_n,
			    GDK_KEY_m,GDK_KEY_BackSpace,GDK_KEY_l,GDK_KEY_r,GDK_KEY_g,GDK_KEY_i,GDK_KEY_p,
			    GDK_KEY_c,GDK_KEY_v,GDK_KEY_e,GDK_KEY_z,GDK_KEY_d,GDK_KEY_b,GDK_KEY_s,GDK_KEY_y,
			    GDK_KEY_f,GDK_KEY_x,GDK_KEY_a,GDK_KEY_w,GDK_KEY_j,0,GDK_KEY_u,GDK_KEY_q,GDK_KEY_k,0};


static gboolean
notTelecode(GdkEventKey *event,gboolean Online)
{
    int n;
   // Allow A-Z
    if((event->keyval >= GDK_KEY_A) && (event->keyval <= GDK_KEY_Z))
	return FALSE;

    // Allow a-z 
    if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
    	return FALSE;

    // Allow 0-9
    if((event->keyval >= GDK_KEY_0) && (event->keyval <= GDK_KEY_9))
	return FALSE;


    if(Online)
    {
	if(ELLIOTTcode)
	{
	    // Allow other valid figureshift characters
	    for(n=0;n<16;n++)
		if(telecodeKeysElliott[n] == event->keyval)
		    return FALSE;
	}
	if(MURRYcode)
	{
	    // Allow other valid figureshift characters
	    for(n=0;n<13;n++)
		if(telecodeKeysMurry[n] == event->keyval)
		    return FALSE;

	}
	
	if(event->keyval == GDK_KEY_Return) return FALSE;
	if(event->keyval == GDK_KEY_space) return FALSE;
	if(event->keyval == GDK_KEY_BackSpace) return FALSE;
    }
    else
    {
	// Allow other valid figureshift characters
	if(gtk_toggle_button_get_active(editorELLIOTTmode))
	{
	    for(n=0;n<16;n++)
		if(telecodeKeysElliott[n] == event->keyval)
		    return FALSE;
	}

	if(gtk_toggle_button_get_active(editorEDSACmode))
	{
	    for(n=0;n<8;n++)
		if(telecodeKeysEDSAC[n] == event->keyval)
		    return FALSE;
	}

	
	// Allow control keys etc
	for(n=0;n<7;n++)
	    if(cursorKeys[n] == event->keyval)
		return FALSE;

	// Allow "#"   It's not telecode, but us used ignore the rest of
	// line (as a comment) when converting to telecode.
	if(gtk_toggle_button_get_active(editorELLIOTTmode))
	    if(event->keyval == GDK_KEY_numbersign) return FALSE;

	// Allow "[" and "]"   They're not telecode, but used to bracket  
	// comments when converting to telecode.
	if(gtk_toggle_button_get_active(editorEDSACmode))
	    if((event->keyval == GDK_KEY_bracketleft) ||
	       (event->keyval == GDK_KEY_bracketright)) return FALSE;
	
    }
    // Ignore everything else !
    return TRUE;


}

// Try to strip excess (>16) runnouts of the beginning and end of the tape.
static void stripRunouts(GByteArray *telecode,guint8 runoutCh)
{
    guint8 *ptr;
    guint headerCounter,trailerCounter,length;

    length = telecode->len;
    ptr = &telecode->data[0];
	    
    headerCounter = 0;

    while((*ptr == runoutCh) && (headerCounter < length))
    {
	headerCounter += 1;
	ptr++;
    }

    //printf("%s head counter = %u length = %u\n",__FUNCTION__,headerCounter,length);

    length = telecode->len;
    ptr = &telecode->data[length-1];
    trailerCounter = 0;

    while((*ptr == runoutCh) && (trailerCounter < length))
    {
	trailerCounter += 1;
	ptr--;
    }

    //printf("%s tail counter = %u length = %u\n",__FUNCTION__,trailerCounter,length);

    // Check if just runouts....
    if(headerCounter != length)
    {
	if(trailerCounter > 16)
	{
	    trailerCounter -= 16;
	    g_byte_array_remove_range(telecode,
				      telecode->len -trailerCounter -1,
				      trailerCounter);
	}

	if(headerCounter > 16)
	{
	    headerCounter -= 16;
	    g_byte_array_remove_range(telecode,
				      0,
				      headerCounter);
	}
    }
    else
    {
	if(trailerCounter > 32)
	{
	    trailerCounter -= 32;
	    g_byte_array_remove_range(telecode,
				      telecode->len -trailerCounter -1,
				      trailerCounter);
	}
    }
}



guint8 shifts[5] = {0x1B,0x1C,0x1D,0x1E,0x1F};
/*
static GByteArray *
convertToTelecode(gboolean fromCursorFlag)*/

static GByteArray *
convertToTelecode(GtkTextIter *start,GtkTextIter *end)
{
    gboolean letters,figures; 
    //GtkTextIter start,end;
    gchar *utf8text,*utf8ptr =  NULL;
    gchar *utf8read,*utf8write,*utf8back;

    gint length,count,depthAtStart;
    GByteArray *telecode;
    gunichar uch;
    gsize utf8CharLength;
    int state;
    letters = figures = FALSE;

    depthAtStart = commentDepthAtIter2(start);

    if(depthAtStart > 0)
    {



    }
    
    utf8ptr = utf8text =  gtk_text_buffer_get_text (editorTextBuffer,start,end,FALSE);
    // Get number of utf8 characters in the text.
    length = (gint) g_utf8_strlen(utf8text,-1);
    

    //printf(" SELECTED TEXT1 %d %d = (%s)\n",depthAtStart,length,utf8ptr);
#if 1
    // Join adjacent comment lines

    utf8read = utf8write = utf8text;
    count = length;
    state = 0;
    
    while(count--)
    {
	uch = g_utf8_get_char(utf8read);
	utf8CharLength = (gsize)  g_utf8_skip[*(guchar *)utf8read];

	//printf("%x %d ",uch,state);

	switch(state)
	{
	case 0:
	    if(uch == U']' )
	    {
		utf8back = utf8write;
		state = 1;
	    }
	    break;

    case 1:
	    if(uch == U'\n' )
		state = 2;
	    else
		state = 0;
	    break;

	case 2:
	    if(uch == U'[' )
		state = 3;
	    else
		state = 0;
	    break;
	}
		
	//printf("%d\n",state);
	
	if(state == 3)
	{
	    utf8write =  utf8back;
	    utf8read += utf8CharLength;
	    state = 0;
	}
	else
	{
	    // Copy the utf8 character.
	    while(utf8CharLength--) *utf8write++ = *utf8read++;
	}
	

    }

    // This is safe as  g_utf8_strlen(utf8text,-1); did not include the terminating null.
    *utf8write = '\0';
    
    length = (gint) g_utf8_strlen(utf8text,-1);
    

    //printf(" SELECTED TEXT2 %d %d = (%s)\n",depthAtStart,length,utf8ptr);
#endif    
    // Start off with worstcase-ish guess at size
    telecode = g_byte_array_sized_new (2 * (guint)length);
    count = 0;

    if(gtk_toggle_button_get_active(editorEDSACmode))
    {
	int depth,prevDepth;
	guint8 sixBits;
	//gunichar uch;
	//gsize utf8CharLength;
	gboolean validTelecode;

	// Add some runnouts so that saved telecode files can be imported into the
	// emulator and loaded into the reader.
	// For uploads into the reader directly extra runnouts won't matter.
	g_byte_array_append(telecode,EDSACrunouts,16);
	

	count = 0;
	depth = 0;
	while(length--)
	{
	    prevDepth = depth;
	    uch = g_utf8_get_char(utf8ptr);
	    utf8CharLength = (gsize)  g_utf8_skip[*(guchar *)utf8ptr];
	
	    switch(uch)
	    {
	    case U'[':
		depth += 1;
		break;
	    case U']':
		depth -= 1;
		break;
	    default:
		break;
	    }

	    //printf("** %x %c %d %d %lu %d\n",uch,uch,depth,prevDepth, utf8CharLength,length);

	    if((depth == 0) && (prevDepth == 0))
	    {
		validTelecode = FALSE;

		for(sixBits = 0; sixBits < 64; sixBits++)
		{
		    if(EDSACToUnicode[sixBits] == 0) continue;

		    if(uch == EDSACToUnicode[sixBits])
		    {
			//printf("Match %x %d\n",uch,sixBits);
			validTelecode = TRUE;
			break;
		    }
		}

		// Save six bits to preserve letters and figures.
		if(validTelecode)
		{
		    g_byte_array_append(telecode,&sixBits,1);
		    count++;
		}
	    }
	    utf8ptr += utf8CharLength;
	}
	
	g_byte_array_append(telecode,EDSACrunouts,16);

	stripRunouts(telecode,0x10);

    }

    if(gtk_toggle_button_get_active(editorELLIOTTmode))
    {
	int depth,prevDepth;
	guint8 fiveBits,sixBits;
	//gunichar uch;
	//gsize utf8CharLength;
	gboolean validTelecode;
	// Add some runnouts so that saved telecode files can be imported into the
	// emulator and loaded into the reader.
	// For uploads into the reader directly extra runnouts won't matter.
	g_byte_array_append(telecode,runouts,16);

	count = 0;
	depth = 0;
	while(length--)
	{
	    prevDepth = depth;
	    uch = g_utf8_get_char(utf8ptr);
	    utf8CharLength = (gsize)  g_utf8_skip[*(guchar *)utf8ptr];
	
	    switch(uch)
	    {
	    case U'[':
		depth += 1;
		break;
	    case U']':
		depth -= 1;
		break;
	    default:
		break;
	    }

	    //printf("** %x %c %d %d %lu %d\n",uch,uch,depth,prevDepth, utf8CharLength,length);

	    if((depth == 0) && (prevDepth == 0))
	    {
		validTelecode = FALSE;

		for(sixBits = 0; sixBits < 64; sixBits++)
		{
		    if(ElliottToUnicode[sixBits] == 0) continue;
		    if(uch == ElliottToUnicode[sixBits])
		    {
			//printf("Match %x %d\n",uch,sixBits);
			validTelecode = TRUE;
			break;
		    }
		}

		if(validTelecode)
		{
		    fiveBits = sixBits & 0x1F;
		    if((sixBits > 58) || (sixBits == 0))
		    {
			// Shift independent
			if(sixBits == 62) 
			{
			    g_byte_array_append(telecode,&shifts[2],1);
			    //printf("1D ");
			    count++;
			}
			g_byte_array_append(telecode,&fiveBits,1);
			//printf("%02X ",fiveBits);
			count++;
		    }	
		    else if((sixBits > 31) && (sixBits <=58))
		    {
			// Letter
			if(!letters)
			{
			    g_byte_array_append(telecode,&shifts[4],1);
			    //printf("1F ");
			    count++;
			    letters = TRUE;
			    figures = FALSE;
			}
			g_byte_array_append(telecode,&fiveBits,1);
			count++;
		    }
		    else if((sixBits > 0) && (sixBits < 32))
		    {
			// Figures
			if(!figures)
			{
			    g_byte_array_append(telecode,&shifts[0],1);
			    //printf("1B ");
			    count++;
			    letters = FALSE;
			    figures = TRUE;
			}
			g_byte_array_append(telecode,&fiveBits,1);
			count++;
		    }
		}
	    }
	    utf8ptr += utf8CharLength;
	}
	g_byte_array_append(telecode,runouts,16);

	stripRunouts(telecode,0x00);
    }
    g_info("\nConverted %d to %d (%d) characters\n",length,count,telecode->len);
    g_free(utf8text);
    return telecode;
}

// Needs documenting !
static
void tagComments(GtkTextBuffer *textBuffer,GtkTextIter *start,GtkTextIter *end)
{
    GtkTextIter iter,openComment,closeComment;
    gunichar uch;
    int state,depth;
    GtkTextTag *tag;



    

    state = 0;
    depth = 0;
    tag = NULL;

    if(start == NULL)
	gtk_text_buffer_get_start_iter(textBuffer,&iter);
    else
	iter = *start;

    //printf("while is %s\n", ((uch = gtk_text_iter_get_char (&iter)) != 0) &&
    //	   ((end == NULL) || !gtk_text_iter_equal(&iter,end)) ? "TRUE" :  "FALSE");
	
    closeComment = iter;
    while( ((uch = gtk_text_iter_get_char (&iter)) != 0) && ((end == NULL) || !gtk_text_iter_equal(&iter,end)))
    {
	//printf("state = %d %x (%c)\n",state,uch,uch&0xFF);
	switch(state)
	{
	case 0:
	    switch(uch)
	    {
	    case U'[':
		openComment = iter;
		state = 1;
		break;

	    case U']':
		openComment = closeComment;
		gtk_text_iter_backward_char(&openComment);
		closeComment = iter;
		gtk_text_iter_forward_char(&closeComment);
		tag = errorTag;
		break;

	    default:

		break;
	    }
	    break;

	case 1:
	    switch(uch)
	    {
	    case U'[':
		depth = 1;
		state = 2;
		break;

	    case U']':
		closeComment = iter;
		gtk_text_iter_forward_char(&closeComment);
		state = 0;
		tag = commentTag;
		break;

	    default:

		break;
	    }
	    break;

	case 2:
	    switch(uch)
	    {
	    case U'[':
		depth += 1;
		break;

	    case U']':
		depth -= 1;
		if(depth == 0)
		    state = 1;
		break;

	    default:
		break;
	    }
	    break;

	case 3:
	    switch(uch)
	    {
	    case U'[':
		break;

	    case U']':
		break;

	    default:
		break;
	    }
	    break;
	default:
	    break;
	}

	if(tag != NULL)
	{
	    gtk_text_buffer_apply_tag(textBuffer,
				      tag,
				      &openComment,
				      &closeComment);
 	    tag = NULL;
	}
	
	gtk_text_iter_forward_char(&iter);
    }

    if(state != 0)
    {
	closeComment = iter;
	gtk_text_buffer_apply_tag(textBuffer,
				      errorTag,
				      &openComment,
				      &closeComment);	
  }
}

/*
Return values:
-1 : Not tagged as a comment 
 0 : Tagged as comment but zero depth so between comments [][]
>0 : Inside [ ] pairs 
*/

static int commentDepthAtIter2(GtkTextIter *location)
{
    gboolean isComment,startsComment;
    int depth;
    GtkTextIter startComment;
    gunichar uch;
    
    isComment = gtk_text_iter_has_tag(location,commentTag);
    startsComment = gtk_text_iter_starts_tag(location,commentTag);

    
    if(startsComment || !isComment) return -1;

    startComment = *location;
    depth = 0;
    
    if(gtk_text_iter_backward_to_tag_toggle(&startComment,commentTag))
    {
	uch = gtk_text_iter_get_char(&startComment);
	    
	while(!gtk_text_iter_equal(&startComment,location))
	{
	    uch = gtk_text_iter_get_char(&startComment);
	    switch(uch)
	    {
	    case U'[':
		depth += 1;
		break;
	    case U']':
		depth -= 1;
		break;
	    default:
		break;
	    }
	    gtk_text_iter_forward_char(&startComment);
	}
    }

    return depth;

}


gboolean
on_commentTestButton_clicked(__attribute__((unused)) GtkButton *button,
			    __attribute__((unused))gpointer data)
{
    //GtkTextIter iter;
    //GtkTextMark *cursor;
    //int depth;

    /* Get the mark at cursor. */
    //cursor = gtk_text_buffer_get_mark (editorTextBuffer, "insert");
    /* Get the iter at cursor. */
    //gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &iter, cursor);
    
    //depth = commentDepthAtIter2(&iter);

    //printf("%s depth = %d\n",__FUNCTION__,depth);

    return GDK_EVENT_PROPAGATE ;
}


static int commentDepthAtIter(GtkTextIter *location,GtkTextIter *start)
{

    GtkTextIter iter,end;
    gunichar uch;
    int depth;
    
    if(start == NULL)
    {
	end = iter = *location;
	gtk_text_iter_backward_to_tag_toggle(&iter,commentTag);
    }
    else
    {
	end = *location;
	iter = *start;
    }

    //printf("%s offsets = %d %d \n",__FUNCTION__,gtk_text_iter_get_offset(&iter),gtk_text_iter_get_offset(&end));
    
    depth = 0;
    
    //if(start != NULL)
    {
	while(!gtk_text_iter_equal(&iter,&end))
	{
	    uch = gtk_text_iter_get_char(&iter);
	    //printf("%s %x\n",__FUNCTION__,uch);
	    switch(uch)
	    {
	    case U'[':
		depth += 1;
		break;
	    case U']':
		depth -= 1;
		break;
	    default:
		break;
	    }
	    gtk_text_iter_forward_char(&iter);
	}
    }
    return depth;
}


// Check that the text to be deleted contains matching pairs of [ and ] so that the
// deletion will not leave the buffer with incorrect comments.
void
on_editorTextBuffer_delete_range(GtkTextBuffer *textbuffer,
				 GtkTextIter   *start,
				 GtkTextIter   *end,
				 __attribute__((unused))gpointer       user_data)
{
    int depth;
    gunichar uchs,uche,uchp;
    GtkTextIter prev,si,ei;
    gboolean OK = FALSE;
    gboolean selected  = FALSE;
    
    prev = *end;
    gtk_text_iter_backward_chars(&prev,2);
    
    uchs = gtk_text_iter_get_char(start);
    uche = gtk_text_iter_get_char(end);
    uchp = gtk_text_iter_get_char(&prev);

    selected = gtk_text_buffer_get_selection_bounds(textbuffer,&si,&ei); 

    /*
    printf("Selected text prev,start,end =  %x %x %x %s\n",uchp,uchs,uche,selected ? "TRUE" :  "FALSE");
    printf("%d %d %d %d\n",
	   gtk_text_iter_get_offset(start),gtk_text_iter_get_offset(end),
	   gtk_text_iter_get_offset(&si),gtk_text_iter_get_offset(&ei));
    */	   

    if(!selected)
    {
	if(Delete)
	{
	    // Check if deleting the ] for a [] pair, and move start to delete the [ as well
	    // Check if deleting the [ for a ][ pair, and move start to delete the ] as well
	    if( ( (uchs == U']') && (uchp == U'[') ) || ( (uchs == U'[') && (uchp == U']') ))
	    {
		*start = prev;
		//printf("DELETE1");
		OK = TRUE;
	    }
	    else
		// Check if deleting the [ for a ][ pair, and move end to delete the ] as well
		// Check if deleting the ] for a [] pair, and move end to delete the ] as well
		if( ( (uchs == U']') && (uche == U'[') ) || ( (uchs == U'[') && (uche == U']') ))
		{
		    gtk_text_iter_forward_char(end);
		    //printf("DELETE2");
		    OK = TRUE;
		}
	
	    Delete = FALSE;
	}

	if(Backspace)
	{
	    // Check if deleting the [ for an empty comment, and move end to delete the ] as well
	    if( ( (uchs == U'[') && (uche == U']') ) ||
		( (uchs == U']') && (uche == U'[') ) )
	    {
		gtk_text_iter_forward_char(end);
		//printf("BACKSPACE1 ");
		OK = TRUE;
	    }
	    else
		// Check if deleting the ] for an empty comment, and start end to delete the [ as well
		if( ( (uchp == U'[') && (uchs == U']') ) ||
		    ( (uchp == U']') && (uchs == U'[') ) )
		{
		    gtk_text_iter_backward_char(start);
		    //printf("BACKSPACE2 ");
		    OK = TRUE;
		}	
	
	    Backspace = FALSE;
	}
    
	depth = commentDepthAtIter(end,start);

	//printf("Comment depth in deleted text = %d\n",depth);

	if((!OK) && (depth != 0))
	{
	    gtk_dialog_run(GTK_DIALOG(deleteAbortedDialog));
	    gtk_widget_hide(deleteAbortedDialog);
	    g_signal_stop_emission_by_name (textbuffer,"delete-range");
	}
    }
    else
    {
	si = *start;
	depth = 0;
	//printf("** ");
	while(!gtk_text_iter_equal(&si,end))
	{
	    uchs = gtk_text_iter_get_char(&si);

	    //printf("0x%2x %d ",uchs,depth);
	    
	    switch(uchs)
	    {
	    case U'[':
		depth += 1;
		break;
	    case U']':
		depth -= 1;
		break;
	    default:
		break;
	    }
	    gtk_text_iter_forward_char(&si);
	}

	//printf(" depth=%d\n",depth);

	if(depth != 0)
	    g_signal_stop_emission_by_name (textbuffer,"delete-range");
    }
}

// Valid keys not included in EDSAC perforator telecode.
static const gunichar *EDSACToUnicodeExtra =
    U"\r\n ";

static gboolean isTelecode(gunichar uch)
{
    gboolean found;

    gunichar const *table;
    gunichar tch;
    
    found = FALSE;

    if(gtk_toggle_button_get_active(editorELLIOTTmode))
    {
	table = ElliottToUnicode;
    }

    if(gtk_toggle_button_get_active(editorEDSACmode))
    {
	// Since cr/lf/sp don't appear in the EDSAC perforator telecode
	// check for them here andreturn TRUE if found
	for(int n = 0; n < 3; n++)
	{
	    if(EDSACToUnicodeExtra[n] == uch)
		return TRUE;
	}
	table = EDSACToUnicode;
    }

    
    for(int n = 0; n < 64; n++)
    {
	tch = table[n];
	if(tch == 0) continue;
	if(tch == uch)
	{
	    //printf("Match %x %d\n",uch,n);
	    found = TRUE;
	    break;
	}
    }
 
    return found;
}


//   TEXT [comment] TEXT  TEXT [comment] TEXT
//TEXT [comment] TEXT  TEXT [comment] TEXT 


/*
Solution from :
https://stackoverflow.com/questions/24837844/modify-text-being-inserted-into-gtk-textbuffer/24837845
*/

void
on_editorTextBuffer_insert_text(GtkTextBuffer *textbuffer,
				     GtkTextIter   *location,
				     gchar         *text,
				     gint           len,
				     __attribute__((unused)) gpointer       user_data)
{
    GtkTextIter insertIter;
    static GtkTextMark *before=NULL,*after=NULL;
    glong utf8CharCount;
    gint utf8CharLength;  // in bytes
    GByteArray *modifiedText = NULL;
    gint bytesRead;
    gunichar uch;
    gboolean tag = FALSE;
    gboolean retag = FALSE;
    gboolean untag = FALSE;
    gboolean oneStepBack = FALSE;
    //gboolean invalid = FALSE;
    gchar *utf8ChPtr;
    gint commentDepth,/*initialCommentDepth,*/previousCommentDepth;

    //printf("%s called\n",__FUNCTION__);
    
    /* Get the iter at insert mark. */
    gtk_text_buffer_get_iter_at_mark (editorTextBuffer,
				      &insertIter,
				      gtk_text_buffer_get_mark (editorTextBuffer, "insert"));

    // Create marks on first call.
    if(before == NULL)
    {
	// Note different gravity so that they move apart as text is inserted
	before = gtk_text_mark_new ("before",TRUE);
	after  = gtk_text_mark_new ("after", FALSE);
	
	gtk_text_buffer_add_mark (editorTextBuffer,before,&insertIter);
	gtk_text_buffer_add_mark (editorTextBuffer,after ,&insertIter);
    }
    else
    {
	// Place marks so that the inserted block can be post-processed if necessary
	gtk_text_buffer_move_mark (editorTextBuffer,before,&insertIter);
	gtk_text_buffer_move_mark (editorTextBuffer,after ,&insertIter);
    }

    modifiedText = g_byte_array_sized_new((guint) len);
    
    utf8CharCount = g_utf8_strlen(text,len);

    commentDepth =  commentDepthAtIter2(location);

    if(commentDepth == 0) untag = TRUE;
    if(commentDepth < 0) commentDepth = 0;
    
    if(utf8CharCount == 1)
    {   // Inserting a single character, so assume it is from keyboard....
	uch = g_utf8_get_char(text);

	if(uch == U'[')
	{
	    g_byte_array_append(modifiedText,(const guint8 *) "[]",2);
	    tag = TRUE;
	    oneStepBack = TRUE;
	    
	}
	else if (uch == U']')
	{
	    if(commentDepth >= 1)
	    {
		g_byte_array_append(modifiedText,(const guint8 *) "][",2);
	    }
	    else
	    {
		g_byte_array_append(modifiedText,(const guint8 *) "[]",2);
		tag = TRUE;
	    }
	    oneStepBack = TRUE;		
	    
	}
	else if(commentDepth >= 1)
	{
	    // Get length of the single character being inserted
	    utf8CharLength =  g_utf8_skip[*(guchar *)text];
	    g_byte_array_append(modifiedText,(guint8 *)text,(guint)utf8CharLength);
	}
	else
	{
	    if(g_unichar_islower(uch))
		uch = g_unichar_toupper(uch);

	    if(isTelecode(uch))
	    {
		gchar utf8Ch[6];
		gint l;
		uch = g_unichar_toupper(uch);
		l = (gsize) g_unichar_to_utf8(uch,utf8Ch);
		g_byte_array_append(modifiedText,(guint8 *)utf8Ch,(guint)l);
	    }
	}
    }
    else
    {   // Inserting multiple characters
	commentDepth = 0; //commentDepthAtIter2(location);
	if(commentDepth <0 ) commentDepth = 0;
	//initialCommentDepth = commentDepth;
	
	utf8ChPtr = text;
	//printf("\n len=%d ",len);
	bytesRead = 0;
	retag = TRUE; 
	
	do
	{
	    previousCommentDepth = commentDepth;
	    uch = g_utf8_get_char(utf8ChPtr);
	    utf8CharLength =  g_utf8_skip[*(guchar *)utf8ChPtr];

	    switch(uch)
	    {
	    case U'[':
		commentDepth += 1;
		break;
	    case U']':
		commentDepth -= 1;
		break;
	    default:
		break;
	    }

	    //if(commentDepth < initialCommentDepth) invalid = TRUE;
	    
	    //printf("*");

	    // Inside a comment so everything is allowed 
	    if((commentDepth > 0) || (previousCommentDepth > 0))
	    {
		g_byte_array_append(modifiedText,(guint8 *)utf8ChPtr,(guint) utf8CharLength);
	    }
	    else
	    {   // Adding outside a comment so telecode only allowed
		if(g_unichar_islower(uch))
		    uch = g_unichar_toupper(uch);

		if(isTelecode(uch))
		{
		    gchar utf8Ch[6];
		    gint l;
		    uch = g_unichar_toupper(uch);
		    l = (gsize) g_unichar_to_utf8(uch,utf8Ch);
		    g_byte_array_append(modifiedText,(guint8 *)utf8Ch,(guint)l);
		}
	    }

	    utf8ChPtr += utf8CharLength;
	    bytesRead += utf8CharLength;
	} while(bytesRead < len);

	//printf("\n");

#if 0
	if((commentDepth != initialCommentDepth) || invalid)
	{
	    // Set array to zero length so nothing will be added.
	    g_byte_array_set_size(modifiedText,0);
	    
	    gtk_dialog_run(GTK_DIALOG(deleteAbortedDialog));
	    gtk_widget_hide(deleteAbortedDialog);
	}
#endif
    }

    // If there is text to insert, do it with the default handler
    if(modifiedText->len > 0)
    {
	// Make the data null terminated
	g_byte_array_append(modifiedText,runouts,1);
	g_signal_handler_block (textbuffer,insertHandlerId);

	gtk_text_buffer_insert_at_cursor (textbuffer,(const char *)modifiedText->data, -1);

	g_signal_handler_unblock (textbuffer,insertHandlerId);
    }


#if 0
    printf("untag = %s tag = %s retag = %s\n",
	   untag ? "TRUE":"FALSE",
	   tag ? "TRUE":"FALSE",
	   retag ? "TRUE":"FALSE");
#endif
    if(untag)
    {
	GtkTextIter starti,endi;
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &starti, before);
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &endi, after);
	gtk_text_buffer_remove_tag(editorTextBuffer,commentTag,&starti,&endi);
    }
    
    if(tag)
    {
	GtkTextIter starti,endi;
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &starti, before);
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &endi, after);
	gtk_text_buffer_apply_tag(editorTextBuffer,
				  commentTag,
				  &starti,
				  &endi);
    }
    
    if(retag)
    {
	GtkTextIter starti,endi;
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &starti, before);
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &endi, after);

	gtk_text_iter_backward_to_tag_toggle (&starti,commentTag);
	gtk_text_iter_forward_to_tag_toggle (&endi,commentTag);

	gtk_text_buffer_remove_tag(editorTextBuffer,commentTag,&starti,&endi);
	tagComments(editorTextBuffer, &starti, &endi);
    }
    
    if(oneStepBack)
    {
	/* Get the iter at insert mark. */
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer,
					  &insertIter,
					  gtk_text_buffer_get_mark (editorTextBuffer, "insert"));

	// Move it back to btween [ and ] 
	gtk_text_iter_backward_char(&insertIter);
	gtk_text_buffer_place_cursor(editorTextBuffer,&insertIter);
	
    }
    // Never let the defult handler be called as everyting has been done
    // in this handler.
    g_signal_stop_emission_by_name (textbuffer,"insert-text");

    g_byte_array_free(modifiedText,TRUE);

    // Must revalidate the location iter otherwise all hell lets loose !
    gtk_text_buffer_get_iter_at_mark (editorTextBuffer,
				      location,
				      gtk_text_buffer_get_mark (editorTextBuffer, "insert"));
}

#if 0
void
on_editorTextBuffer_insert_textOLD(GtkTextBuffer *textbuffer,
				     GtkTextIter   *location,
				     gchar         *text,
				     gint           len,
				     __attribute__((unused)) gpointer       user_data)
{
    const gchar *comment =  "[]";
    const gchar *split =  "][";
    gchar *end,*start,*next;
    const gchar *toInsert;
    gunichar uch;
    GtkTextIter iter,startComment;
    gboolean modified = FALSE;
    gboolean isComment = FALSE;
    gboolean startsComment = FALSE;
    gboolean untag = FALSE;
    gboolean tag = FALSE;
    gboolean retag = FALSE;
    gboolean invalid = FALSE;
    gboolean oneStepBack = FALSE;
    int depth,prevDepth,startDepth;
    static GtkTextMark *before=NULL,*after=NULL;
    GtkTextIter starti,endi;
    GtkTextMark *cursor;
    gsize utf8CharLength;
    gchar utf8Ch[7];   // 7 so a 6 byte utf8 sequence can be null terminated.
    GByteArray *modifiedText = NULL;
    
    //printf("%s called %d %s %p\n",__FUNCTION__,len,text,g_utf8_skip);
    
    /* Get the mark at cursor. */
    cursor = gtk_text_buffer_get_mark (editorTextBuffer, "insert");
    /* Get the iter at cursor. */
    gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &iter, cursor);


    // Create marks on first call.
    if(before == NULL)
    {
	// Note different gravity so that they move apart as text is inserted
	before = gtk_text_mark_new ("before",TRUE);
	after  = gtk_text_mark_new ("after", FALSE);
	
	gtk_text_buffer_add_mark (editorTextBuffer,before,&iter);
	gtk_text_buffer_add_mark (editorTextBuffer,after ,&iter);
    }
    else
    {
	// Place marks so that the inserted block can be post-processed if necessary
	gtk_text_buffer_move_mark (editorTextBuffer,before,&iter);
	gtk_text_buffer_move_mark (editorTextBuffer,after ,&iter);
    }
    
    // Take a copy of the insertion iter
    iter = *location;

    uch = gtk_text_iter_get_char(&iter);
    isComment = gtk_text_iter_has_tag(&iter,commentTag);
    startsComment = gtk_text_iter_starts_tag(&iter,commentTag);
    utf8CharLength = g_utf8_strlen(text,len);
    /*
    printf("ch at iter = %x  isComment=%s startsComment=%s utf8chars=%d bytes=%d\n",
	   uch,
	   isComment ? "TRUE" : "FALSE",
	   startsComment ? "TRUE" : "FALSE",
	   utf8CharLength,len);
    */
    toInsert = text;


/* TODO    
    if(isComment && !startsComment)
    {
	startComment = iter;
	if(gtk_text_iter_backward_to_tag_toggle(&startComment,commentTag))
	{
	    uch = gtk_text_iter_get_char(&startComment);
	    printf("Comment starts with %x\n",uch);
	    depth = 0;
	    untag = FALSE;
	    
	    while(!gtk_text_iter_equal(&startComment,&iter))
	    {
		uch = gtk_text_iter_get_char(&startComment);
		switch(uch)
		{
		case 0x5b:
		    depth += 1;
		    break;
		case 0x5d:
		    depth -= 1;
		    break;
		default:
		    break;
		}
		gtk_text_iter_forward_char(&startComment);
	    }

	    // Deal with inserting first character between two comments
	    //  [xxxx][xxxx]  -> [xxxx]I[xxxx]
	    if(depth == 0)
	    {
		untag = TRUE;
		// Don't let the default handler do the insertions
		modified = TRUE;
	    }
	    printf(">>>>>>>>>> Depth = %d\n",depth);
	}
    }
*/
    start = text;
    end = &text[len+1];
    
    next = g_utf8_find_next_char (start,end);
    
    // Insert single character  vs. Paste a block
    uch = g_utf8_get_char(next);
    //printf("next = %p uch = %x\n",next,uch);
    if(uch == 0)
	//if(next == NULL)
    {
	// Key press / single character
	//printf("SIngle char \n");

	if(startsComment) isComment = FALSE;
	
	uch = g_utf8_get_char(start);

	// [
	if(uch == 0x5b)
	{
	    toInsert = comment;
	    modified = TRUE;
	    tag = TRUE;
	    oneStepBack = TRUE;

	}

	// ]
	else if(uch == 0x5d)
	{
	    if(isComment && (depth >1))
	    {
		toInsert = split;
		modified = TRUE;
	    }
	    else
	    {
		toInsert = comment;
		modified = TRUE;
		tag = TRUE;
	    }
	    oneStepBack = TRUE;
	}
	else if((!isComment) && (g_unichar_islower(uch)))
	{
	    uch = g_unichar_toupper(uch);
	    utf8CharLength = (gsize) g_unichar_to_utf8(uch,utf8Ch);
	    utf8Ch[utf8CharLength] = '\0';
	    toInsert = utf8Ch;
	    modified = TRUE;
	}
	else if((!isComment) && !isTelecode(uch))
	{
	    g_signal_stop_emission_by_name (textbuffer,"insert-text");
	}
    }
    else
    {
	// Pasted text
	
	// Place to put modified text
	modifiedText = g_byte_array_sized_new((guint) len);

	startDepth = depth = commentDepthAtIter(location,NULL);
	
	//printf("Pasted Text %d\n",depth);

	end++;
	//if(!isComment)
	    retag = TRUE; 
	    

	do
	{
	    prevDepth = depth;
	    uch = g_utf8_get_char(start);
	    utf8CharLength = (gsize)  g_utf8_skip[*(guchar *)start];
	    //printf("uch = %x %c (%lu) %d\n",uch,uch,utf8CharLength,depth);
	    switch(uch)
	    {
	    case 0x5b:
		depth += 1;
		break;
	    case 0x5d:
		depth -= 1;
		break;
	    default:
		break;
	    }
	    if(depth < 0) invalid = TRUE;


	    //printf("********* depth = %d prevDepth = %d **************** \n",
	    depth,prevDepth);
	    
	    if((depth > 0) || (prevDepth > 0))
	    {
		utf8CharLength = (gsize) g_unichar_to_utf8(uch,utf8Ch);
		g_byte_array_append(modifiedText,(guint8 *)utf8Ch,(guint) utf8CharLength);
	    }
	    else
	    {
		uch = g_unichar_toupper(uch);

		
/*
		found = FALSE;
		
		for(int n = 0; n < 64; n++)
		{
		    //if(convert2[n] == NULL) continue;
		    if(ElliottToUnicode[n] == 0) continue;
		    //printf("%p %p %d\n",convert2[n],utf8Ch,utf8CharLength);
		    //if(strncmp(convert2[n],utf8Ch,utf8CharLength) == 0)
		    if(ElliottToUnicode[n] == uch)
		    {
			printf("Match %x %d\n",uch,n);
			found = TRUE;
			break;
		    }

		}
*/
		if(isTelecode(uch))
		{
		    utf8CharLength = (gsize) g_unichar_to_utf8(uch,utf8Ch);
		    g_byte_array_append(modifiedText,(guint8 *)utf8Ch,
					(guint) utf8CharLength);
		}
	    }

	    
	    
	    start = next;
	} while ( (next = g_utf8_find_next_char (start,end)) != NULL);

	g_byte_array_append(modifiedText,runouts,1);
	
	
	if((depth == startDepth) && !invalid)
	{
	    modified = TRUE;
	    toInsert = (gchar *) modifiedText->data;
	}
	else
	{
	    gtk_dialog_run(GTK_DIALOG(deleteAbortedDialog));
	    gtk_widget_hide(deleteAbortedDialog);
	    g_signal_stop_emission_by_name (textbuffer,"insert-text");
	    modified = FALSE;
	    untag = FALSE;
	}

	
    }
	
//printf("\n");
	

    
    if(modified)
    {
	if(toInsert != NULL)
	{
	    //gint offsetBefore,offsetAfter;
	    
	    //offsetBefore = gtk_text_iter_get_offset(&iter);
	    
	    g_signal_handler_block (textbuffer,insertHandlerId);

	    gtk_text_buffer_insert_at_cursor (textbuffer,toInsert, -1);

	    g_signal_handler_unblock (textbuffer,insertHandlerId);

	    
	}
	g_signal_stop_emission_by_name (textbuffer,"insert-text");
    }


    // Remove commentTag from  first characterinserted between two comments
    //[xxxx][xxxx]  -> [xxxx]I[xxxx]

    if(untag)
    {
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &starti, before);
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &endi, after);
	gtk_text_buffer_remove_tag(editorTextBuffer,commentTag,&starti,&endi);
    }


    if(tag)
    {
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &starti, before);
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &endi, after);
	gtk_text_buffer_apply_tag(editorTextBuffer,
				  commentTag,
				  &starti,
				  &endi);
    }

    if(retag)
    {
	//printf("RETAGING\n");
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &starti, before);
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &endi, after);

	gtk_text_iter_backward_to_tag_toggle (&starti,commentTag);
	gtk_text_iter_forward_to_tag_toggle (&endi,commentTag);

	gtk_text_buffer_remove_tag(editorTextBuffer,commentTag,&starti,&endi);
	tagComments(editorTextBuffer, &starti, &endi);
	//printf("RETAGGED\n");
    }


    
    if(oneStepBack)
    {
	/* Get the mark at cursor. */
	cursor = gtk_text_buffer_get_mark (editorTextBuffer, "insert");
	/* Get the iter at cursor. */
	gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &iter, cursor);
	// Move it back to btween [ and ] 
	gtk_text_iter_backward_char(&iter);
	gtk_text_buffer_place_cursor(editorTextBuffer,&iter);
	
    }
}

#endif
    

static gboolean commentCheck(GtkTextIter *start,GtkTextIter *end)
{

    GtkTextIter iter;
    gunichar uch;
    int depth,zeros,prevDepth;
    gboolean valid;
    
    iter = *start;
    
    //printf("%s offsets = %d %d \n",__FUNCTION__,gtk_text_iter_get_offset(start),gtk_text_iter_get_offset(end));  
    zeros = -1;
    prevDepth = depth = 0;
    valid = TRUE;
    
    while(!gtk_text_iter_equal(&iter,end))
    {
	prevDepth = depth;
	uch = gtk_text_iter_get_char(&iter);
	
	switch(uch)
	{
	case U'[':
	    depth += 1;
	    break;
	case U']':
	    depth -= 1;
	    break;
	default:
	    break;
	}
	if(depth == 0) zeros += 1;

	if((depth == 1) && (prevDepth == 1))
	{
	    if(!isTelecode(uch)) valid = FALSE;
	    //printf("%x %c is code %s\n",uch,uch,valid  ? "TRUE" : "FALSE");
	}
	
	//printf("%s %x %d %d %d \n",__FUNCTION__,uch,depth,prevDepth,zeros);
	gtk_text_iter_forward_char(&iter);
    }
    
    return ((depth == 0) && (zeros == 0) && valid);
}



gboolean on_commentUnwrapButton_clicked(__attribute__((unused)) GtkButton *widget,
					__attribute__((unused)) gpointer data)
{
    GtkTextIter si,ei,pi; 
    gboolean selected  = FALSE,commentOk;
    int depthStart,depthEnd;
    gunichar uchs,uchp;
    
    selected = gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);

    if(!selected)
	return GDK_EVENT_PROPAGATE; 
    
    pi = ei;
    gtk_text_iter_backward_char(&pi);
    
    uchs = gtk_text_iter_get_char(&si);
    uchp = gtk_text_iter_get_char(&pi);

    //printf("%s %x %x\n",__FUNCTION__,uchs,uchp);

    depthStart = commentDepthAtIter2(&si);
    depthEnd   = commentDepthAtIter2(&ei);
    if(depthStart < 0) depthStart = 0;
    if(depthEnd < 0) depthEnd = 0;    

    
    //printf("%s selected = %s %d %d\n",__FUNCTION__,selected ? "TRUE" :  "FALSE",
    //	   depthStart,depthEnd);
    
    commentOk = commentCheck(&si,&ei);

    //printf("commentOK = %s uchs = %c uchp = %c\n", commentOk ? "TRUE" : "FALSE", uchs, uchp);
    
    if( !commentOk || (uchs != '[') || (uchp != ']') )
    {
	gtk_dialog_run(GTK_DIALOG(deleteAbortedDialog));
	gtk_widget_hide(deleteAbortedDialog);	
	return GDK_EVENT_STOP; 
    }

    if(depthStart == 0)
    {
	gtk_text_buffer_remove_tag(editorTextBuffer,commentTag,&si,&ei);
    }
    
    g_signal_handler_block (editorTextBuffer,deleteHandlerId);
    
    gtk_text_buffer_delete(editorTextBuffer,&pi,&ei);

    selected = gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);

    pi = si;
    gtk_text_iter_forward_char(&pi);
    gtk_text_buffer_delete(editorTextBuffer,&si,&pi);

    g_signal_handler_unblock (editorTextBuffer,deleteHandlerId);

    if(depthStart == 0)
    {
	selected = gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);
	gtk_text_iter_backward_char(&si);
	tagComments(editorTextBuffer, &si, &ei);
    }
    
    gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);
    gtk_text_buffer_place_cursor(editorTextBuffer,&ei);

    return GDK_EVENT_PROPAGATE; 
}


gboolean on_commentWrapButton_clicked(__attribute__((unused)) GtkButton *widget,
				      __attribute__((unused)) gpointer data)
{
    GtkTextIter si,ei;
    gboolean selected  = FALSE;
    int depthStart,depthEnd;

#if 1
    selected = gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);

    if(!selected)
	return GDK_EVENT_PROPAGATE; 
    
    depthStart = commentDepthAtIter2(&si);
    depthEnd   = commentDepthAtIter2(&ei);
    if(depthStart < 0) depthStart = 0;
    if(depthEnd < 0) depthEnd = 0; 
    
    //printf("%s selected = %s %d %d\n",__FUNCTION__,selected ? "TRUE" :  "FALSE",
    //   depthStart,depthEnd);

    if(depthStart != depthEnd)
    {
	gtk_dialog_run(GTK_DIALOG(deleteAbortedDialog));
	gtk_widget_hide(deleteAbortedDialog);	
	return GDK_EVENT_STOP; 
    }
  
    g_signal_handler_block (editorTextBuffer,insertHandlerId);
     
    gtk_text_buffer_insert (editorTextBuffer,&si,"[",-1);

    selected = gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);
    gtk_text_buffer_insert (editorTextBuffer,&ei,"]",-1);

    g_signal_handler_unblock (editorTextBuffer,insertHandlerId);

    selected = gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);
    gtk_text_iter_backward_char(&si);
    tagComments(editorTextBuffer, &si, &ei);

    gtk_text_buffer_get_selection_bounds(editorTextBuffer,&si,&ei);
    gtk_text_buffer_place_cursor(editorTextBuffer,&ei);

#endif
    return GDK_EVENT_PROPAGATE; 
}


gboolean on_editorTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
					   GdkEventKey *event)
{
    GtkTextIter iter;
    GtkTextMark *cursor;

    //printf("%s (%x)\n",__FUNCTION__,event->keyval);
    
    /* Get the mark at cursor. */
    cursor = gtk_text_buffer_get_mark (editorTextBuffer, "insert");
    /* Get the iter at cursor. */
    gtk_text_buffer_get_iter_at_mark (editorTextBuffer, &iter, cursor);
 
    if(event->keyval == GDK_KEY_BackSpace)
    {
	Backspace = TRUE;
    }
    
    if(event->keyval == GDK_KEY_Delete)
    {
	Delete = TRUE;
    } 

    if(event->keyval == GDK_KEY_F1)
    {
	on_commentWrapButton_clicked(NULL,NULL);
    }

    if(event->keyval == GDK_KEY_F2)
    {
	on_commentUnwrapButton_clicked(NULL,NULL);
    }

    return GDK_EVENT_PROPAGATE;
}


gboolean
on_editorNewButton_clicked(__attribute__((unused)) GtkButton *button,
			   __attribute__((unused))gpointer data)
{
    GtkTextIter start,end;

    gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
    gtk_text_buffer_get_end_iter (editorTextBuffer,&end);

    gtk_text_buffer_delete (editorTextBuffer,&start,&end);
    
    gtk_widget_set_sensitive(editorSaveButton,FALSE);
    gtk_label_set_text(GTK_LABEL(editorFrameLabel),"Tape Editor");

    return  GDK_EVENT_PROPAGATE;
}

/* It used to workk like this ....
  If a file with     ".utf8" extension is selected it is used 
  If a file without  ".utf8" extansion is selected and a ".utf8" version exists
     then ask user to choose which one to use
     otherwise load the telecode vversion 
 */

gchar BOMutf16le[2] = {'\xff','\xfe'};
gchar BOMutf16be[2] = {'\xfe','\xff'};
gchar BOMutf8[3] = {'\xef','\xbb','\xbf'};
gchar BOMele1[3] = {'\x00','\x00','\x00'};
gchar BOMele2[3] = {'\x80','\x80','\x80'};
gchar BOMedsac1[3] = {'\x10','\x10','\x10'};
gchar BOMedsac2[3] = {'\x90','\x90','\x90'};



static gchar *BOMs[] = {BOMutf16le,BOMutf16be,BOMutf8,
		 BOMele1,BOMele2,
		 BOMedsac1,BOMedsac2};
/*
static const gchar *BOMnames[] = {"UTF-16LE","UTF-16BE","UTF-8",
		     "ELLIOTT","ELLIOTT",
		     "EDSAC","EDSAC"};
*/
gboolean
on_editorOldButton_clicked(__attribute__((unused)) GtkButton *widget,
			   __attribute__((unused))gpointer data)
{
    gint res;
    gsize length;
    gsize bytesWritten,bytesRead;
    gboolean returnFlag;
    char *filename = NULL;
    gchar *dataBuffer = NULL;
    gboolean validUtf8;
    gchar *converted;
    
    //printf("%s called\n",__FUNCTION__);
    returnFlag = GDK_EVENT_PROPAGATE;
    converted = NULL;

    // Get a filename from a file choosed dialog.
    res = gtk_dialog_run (GTK_DIALOG(loadFileChooserDialog));
    gtk_widget_hide(loadFileChooserDialog);

    if (res == GTK_RESPONSE_OK)
    {
	GFile *gf;
	GError *error = NULL; 
	int type;
	gboolean BOMmatch;

	filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER (loadFileChooserDialog));
	
	gf = g_file_new_for_path(filename);
	
	g_file_load_contents (gf,NULL,&dataBuffer,
			      &length,NULL,&error);
	
	g_object_unref(gf);

	validUtf8 = g_utf8_validate(dataBuffer,(gssize) length,NULL);
	
	BOMmatch = FALSE;

	// Check first three bytes of the file for a utf "Byte Order Mark" or Elliott
	// or EDSAC telecode runouts.
	
	for(type = 0; type < 7; type++)
	{
	    if(strncmp(dataBuffer,BOMs[type],type < 2 ? 2 : 3 ) == 0)
	    {
		BOMmatch = TRUE;
		//printf("file %s is type %d\n",filename,type);
		break;
	    }
	}

	if(!BOMmatch)
	{
	    if(validUtf8)
	    {
		type = 2;
		BOMmatch = TRUE;
	    }
	    else
	    {
		//printf("%s : Unrecognised file format \n",filename);
		returnFlag =  FALSE;
		goto cleanUp;
	    }
	}

	
	switch(type)
	{
	case 0:  // utf16le
	    converted = g_convert(dataBuffer,(gssize)length,"UTF-8","UTF-16LE",
				  &bytesRead,&bytesWritten,&error);
	    break;
	case 1:  // utf16be
	    converted = g_convert(dataBuffer,(gssize)length,"UTF-8","UTF-16BE",
				  &bytesRead,&bytesWritten,&error);
	    break;
	case 2:  // utf8
	    converted = dataBuffer;
	    bytesWritten = length;
	    dataBuffer = NULL;
	    break;
	case 3:  // Elliott
	case 4:
	  
	    if(isBinaryTape(dataBuffer,length))
	    {
		res = gtk_dialog_run(GTK_DIALOG(editBinaryDialog));
		gtk_widget_hide(editBinaryDialog);
		
		if(res != GTK_RESPONSE_OK)
		{
		    returnFlag = GDK_EVENT_STOP;
		    goto cleanUp;
		}
	    }
	    
	    converted = ElliottToUtf8(dataBuffer,length,&bytesWritten);

	    bytesWritten -= 1;  // Dont include the null when writting to text buffer
	    /*
	      printf("bytesWritten = %d\n",bytesWritten);
		    
	      for(int n = 0; n < bytesWritten; n++)
	      {
	      printf("converted[%d] = %02X\n",n,converted[n] & 0xFF);
	      }
	    */
		    
	    /*
	    if(g_utf8_validate(converted,(gssize)bytesWritten-1,NULL))
	    {
		//printf("Convertion OK\n");
	    }
	    else
	    {
		//printf("Convertion FAILED\n");
	    }
	    */	    
	    break;

	case 5:  // EDSAC
	case 6:


	    converted = EDSACtoUft8(dataBuffer,length,&bytesWritten);
	    bytesWritten -= 1;  // Dont include the null when writting to text buffer
	    /*
	    if(g_utf8_validate(converted,(gssize)bytesWritten-1,NULL))
	    {
		//printf("Convertion OK\n");
	    }
	    else
	    {
		//printf("Convertion FAILED\n");
	    }
	    */
	    break;
	default:

	    break;
	}

	//printf("Text was (%s)\n",converted);
	gtk_text_buffer_insert_at_cursor(editorTextBuffer,converted,(gint)bytesWritten);

	
    cleanUp:
	if(dataBuffer != NULL)	g_free(dataBuffer);
	if(converted != NULL) g_free(converted);

    }
    if(filename != NULL) g_free(filename);
    return returnFlag;
}

// Old code for reference
#if 0    
    if (res == GTK_RESPONSE_OK)
    {
	filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER (loadFileChooserDialog));

	// Check for ".utf8" extension, and  if present load the file directly into the text buffer
	const char *dot = strrchr(filename, '.');
	if((dot != NULL) && (strncmp(dot,".utf8",5) == 0))
	{
	    //printf("This is a utf8 file\n");
	    useUTF8 = TRUE;
	}
	else
	{
	    // Add ".utf8" extension and check if it exists.

	    utf8Filename = g_string_new(filename);
	    g_string_append(utf8Filename,".utf8");

	    if(access(utf8Filename->str,R_OK) == 0)
	    {
		res = gtk_dialog_run(GTK_DIALOG(chooseFormatDialog));
		gtk_widget_hide(chooseFormatDialog);
		
		if(res == GTK_RESPONSE_NO)
		{ // Use UTF8 file
		    useUTF8 = TRUE;
		}
	    }
	}

	if(useUTF8)
	{
	    char *fn;

	    if(utf8Filename != NULL)
	    {
		fn = utf8Filename->str;
	    }
	    else
	    {
		fn = filename;
	    }

	    {
		GFile *gf;
		GError *error = NULL; 

		gf = g_file_new_for_path(fn);

		g_file_load_contents (gf,NULL,&dataBuffer,
			      &length,NULL,&error);
		
		g_object_unref(gf);
		gtk_text_buffer_insert_at_cursor(editorTextBuffer,dataBuffer,(gint) length);
		g_free(dataBuffer);
		dataBuffer = NULL;

		tagComments(editorTextBuffer,NULL,NULL);
		
	    }
	}
	else
	{
	    {
		GFile *gf;
		GError *error = NULL;
		gf = g_file_new_for_path(filename);

		g_file_load_contents (gf,NULL,&dataBuffer,
				      &length,NULL,&error);
		mask5holes(dataBuffer,length);
		//TODO Check error returned

		g_object_unref(gf);
	    }

	    if(!isTelecodeTape(dataBuffer,length))
	    {
		res = gtk_dialog_run (GTK_DIALOG (notTelecodeDialog));
		gtk_widget_hide(notTelecodeDialog);
		returnFlag = GDK_EVENT_STOP;
		goto cleanUp;
	    }

	    if(isBinaryTape(dataBuffer,length))
	    {
		res = gtk_dialog_run(GTK_DIALOG(editBinaryDialog));
		gtk_widget_hide(editBinaryDialog);

		if(res == GTK_RESPONSE_CANCEL)
		{
		    returnFlag =  GDK_EVENT_STOP;
		    goto cleanUp;
		}
	    }
	
	    letters = figures = FALSE;

	    for(index=0;index<length;index++)
	    {
		txt = NULL;
		ch = dataBuffer[index] & 0x1F;
		if(ch >= 0x1B)
		{
		    switch(ch)
		    {
		    case 0x1B:
			figures = TRUE;
			letters = FALSE;
			txt = NULL;
			break;
		    case 0x1C:
			txt = " ";
			break;
		    case 0x1D:
			txt = "\n";
			break;
		    case 0x1E:
			break;
		    case 0x1F:
			figures = FALSE;
			letters = TRUE;
			break;
		    }
		}
		else
		{
		    if( letters || figures)
		    {
			if(letters) ch += 32;
			txt = convert2[ch];
		    }
		}

		if(txt != NULL)
		{
		    gtk_text_buffer_insert_at_cursor(editorTextBuffer,txt,-1);
		}
	    }
	}
cleanUp:
	if(dataBuffer) g_free(dataBuffer);
	dataBuffer = NULL;
	if(filename) g_free(filename);
	if(utf8Filename) g_string_free(utf8Filename,TRUE);
			 
    }
    return returnFlag ;
#endif


gboolean
on_editorUploadButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data)
{
    GByteArray *telecode;
    gchar value[3];
    GError *error = NULL;
    gsize written;
    GtkTextIter start,end;
    
    if(editorUploadBuffer != NULL)
	g_free(editorUploadBuffer);
    
    // If there is selected text, use that, else use whole buffer.
    if(!gtk_text_buffer_get_selection_bounds (editorTextBuffer,&start,&end))
    {
	gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
	gtk_text_buffer_get_end_iter (editorTextBuffer,&end);
    }
    
    telecode = convertToTelecode(&start,&end);
    
    if(gtk_toggle_button_get_active(editorEDSACmode))
	readerEchoType = 5;
    if(gtk_toggle_button_get_active(editorELLIOTTmode))
	readerEchoType = 3;

#if 1
    
    editorUploadLength = telecode->len;
    editorUploadBuffer = (gchar *) g_byte_array_free (telecode,FALSE);
    editorUploaded = 0;

    uploadLengthp = &editorUploadLength;
    uploadedp = &editorUploaded;
    uploadBuffer = editorUploadBuffer;

    value[0] = '\x80';
    value[1] = 0x00;
    value[2] = 0x00;
    
    g_io_channel_write_chars(E803_channel,value,3,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    // Save button states
    fileUploadWasSensitive   = gtk_widget_get_sensitive(fileUploadButton);
    editorUploadWasSensitive = gtk_widget_get_sensitive(editorUploadButton);
    editorUploadFromCursorWasSensitive = gtk_widget_get_sensitive(editorUploadFromCursorButton);
    
    gtk_widget_set_sensitive(fileUploadButton,FALSE);
    gtk_widget_set_sensitive(editorUploadFromCursorButton,FALSE);
    gtk_widget_set_sensitive(editorUploadButton,FALSE);
#endif
    return GDK_EVENT_PROPAGATE ;
}

gboolean
on_editorUploadFromCursorButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data)
{
    GByteArray *telecode;
    gchar value[3];
    GError *error = NULL;
    gsize written;
    GtkTextIter start,end;
    GtkTextMark *tm;
    
    if(editorUploadBuffer != NULL)
	g_free(editorUploadBuffer);



    
    tm = gtk_text_buffer_get_insert (editorTextBuffer);

    gtk_text_buffer_get_iter_at_mark (editorTextBuffer,
				      &start,
				      tm);
    gtk_text_buffer_get_end_iter (editorTextBuffer,&end);
    
    telecode = convertToTelecode(&start,&end);

    if(gtk_toggle_button_get_active(editorEDSACmode))
	readerEchoType = 5;
    if(gtk_toggle_button_get_active(editorELLIOTTmode))
	readerEchoType = 3;
    
    editorUploadLength = telecode->len;
    editorUploadBuffer = (gchar *) g_byte_array_free (telecode,FALSE);
    editorUploaded = 0;

    uploadLengthp = &editorUploadLength;
    uploadedp = &editorUploaded;
    uploadBuffer = editorUploadBuffer;

    value[0] = (gchar) 0x80;
    value[1] = 0x00;
    value[2] = 0x00;
    
    g_io_channel_write_chars(E803_channel,value,3,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    // Save button states
    fileUploadWasSensitive   = gtk_widget_get_sensitive(fileUploadButton);
    editorUploadWasSensitive = gtk_widget_get_sensitive(editorUploadButton);
    editorUploadFromCursorWasSensitive = gtk_widget_get_sensitive(editorUploadFromCursorButton);
    
    gtk_widget_set_sensitive(fileUploadButton,FALSE);
    gtk_widget_set_sensitive(editorUploadButton,FALSE);
    gtk_widget_set_sensitive(editorUploadFromCursorButton,FALSE);
    return GDK_EVENT_PROPAGATE ;
}



const gchar *exts[] = {
    ".ele",".edsac",".utf8"};

gboolean
on_editorSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))	gpointer data)
{
    gint res;
    
    //printf("%s called\n",__FUNCTION__);

    if(editorTapeName == NULL)
    {
	editorTapeName = g_string_sized_new(100);
    }
    
    res = gtk_dialog_run (GTK_DIALOG (saveFileChooserDialog));

    gtk_widget_hide(saveFileChooserDialog);
    
    if (res == GTK_RESPONSE_OK)
    {
	//GString *title;
	gchar *path;
	gchar *file;
	gchar *fileExtension;
	gchar *filename;
	gboolean cont;
	
	//if(editorTapeName != NULL)
	//    g_free (editorTapeName);
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (saveFileChooserDialog));

	file = g_path_get_basename(filename);
	path = g_path_get_dirname(filename);

	g_free(filename);

	
	while((fileExtension = g_utf8_strrchr(file,-1,U'.')) != NULL)
	{
	    cont = FALSE;
	    for(int n = 0; n < 3; n++)
	    {
		if(g_utf8_collate(fileExtension,exts[n]) == 0)
		{
		    *fileExtension = '\0';
		    cont = TRUE;
		    break;
		}
	    }
	    if(!cont) break;
	    
	}
#if 0
	fileExtension = g_utf8_strrchr(file,-1,U'.');

	if(fileExtension != NULL)
	{
	    //printf("Extension = %s\n",fileExtension);
	    for(int n = 0; n < 3; n++)
	    {
/*
		printf("comparing %s and %s gives %d\n",
		       fileExtension,exts[n],
		       g_utf8_collate(fileExtension,exts[n]));
*/
		if(g_utf8_collate(fileExtension,exts[n]) == 0)
		{
		    *fileExtension = '\0';
		    break;
		}
	    }
	}
#endif


	
	g_string_printf(editorTapeName,"Tape is %s in %s",file,path); //editorTapeName);
	gtk_label_set_text(GTK_LABEL(editorFrameLabel),editorTapeName->str);

	g_string_printf(editorTapeName,"%s/%s",path,file);

	
	
	gtk_widget_set_sensitive(editorSaveButton,TRUE);
	g_free(path);
	g_free(file);
    }
    return GDK_EVENT_PROPAGATE ;
}


// Also save the utf8 text added so that comments can be preserved
gboolean
on_editorSaveButton_clicked(__attribute__((unused)) GtkButton *button,
			    __attribute__((unused))gpointer data)
{
    GByteArray *telecode;
    GString *utf8FileName,*telecodeFileName;
    GtkTextIter start,end;
    guchar *utf8text;
    gsize slength;

    //gchar *fileExtension;

    // If there is selected text, use that, else use whole buffer.
    if(!gtk_text_buffer_get_selection_bounds (editorTextBuffer,&start,&end))
    {
	gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
	gtk_text_buffer_get_end_iter (editorTextBuffer,&end);
    }
    
    telecode = convertToTelecode(&start,&end);

    telecodeFileName = g_string_new(editorTapeName->str);
    if(gtk_toggle_button_get_active(editorELLIOTTmode))
    {
	g_string_append(telecodeFileName,".ele");
    }
    else
    {
	g_string_append(telecodeFileName,".edsac");
    }
    
    {
	GFile *gf;
	GError *error = NULL;
	    
	gf = g_file_new_for_path(telecodeFileName->str);

	g_file_replace_contents (gf,(char *)telecode->data,telecode->len,
				 NULL,FALSE,G_FILE_CREATE_NONE,
				 NULL,NULL,&error);
	g_object_unref(gf);
    }	
    g_string_free(telecodeFileName,TRUE);
    g_byte_array_free(telecode,TRUE);


	
    utf8FileName = g_string_new(editorTapeName->str);
    g_string_append(utf8FileName,".utf8");


    
    //gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
    //gtk_text_buffer_get_end_iter (editorTextBuffer,&end);

    utf8text =  (guchar *) gtk_text_buffer_get_text (editorTextBuffer,&start,&end,FALSE);
    // GOTCHA !!!  This returns uft8 character count NOT the byte count !!!
    //length = gtk_text_buffer_get_char_count (editorTextBuffer);

    // strlen is OK because utf8 does not have any zero bytes in it !
    slength = strlen((const char *)utf8text);

    {
	GFile *gf;
	GError *error = NULL;

	gf = g_file_new_for_path(utf8FileName->str);

	g_file_replace_contents (gf,(gchar *) utf8text,slength,
				 NULL,FALSE,G_FILE_CREATE_NONE,
				 NULL,NULL,&error);
	g_object_unref(gf);
    }	

    g_string_free(utf8FileName,TRUE);
    g_free(utf8text);
    
    return GDK_EVENT_PROPAGATE ;
}


gboolean
on_editorELLIOTTmode_toggled(__attribute__((unused))GtkWidget *tape,
				    __attribute__((unused))GdkEventButton *event,
				    __attribute__((unused))gpointer data)
{
    //printf("%s called\n",__FUNCTION__);
    
    return GDK_EVENT_PROPAGATE ;

}


gboolean
on_editorEDSACmode_toggled(__attribute__((unused))GtkWidget *tape,
				    __attribute__((unused))GdkEventButton *event,
				    __attribute__((unused))gpointer data)
{
    //printf("%s called\n",__FUNCTION__);
    return GDK_EVENT_PROPAGATE ;

}    


/********************* Tape Drawing Area *************************/

#define HOLEWIDTH 8
uint8_t rowToBit[6] = {0x01,0x02,0x04,0x80,0x08,0x10};
static uint8_t hole[8][8] =
{{0xFF,0xFF,0xFF,0xEF,0xEF,0xFF,0xFF,0xFF},
 {0xFF,0xDF,0x9F,0x70,0x70,0x9F,0xDF,0xFF},
 {0xFF,0x9F,0x20,0x00,0x00,0x20,0x9F,0xFF},
 {0xEF,0x70,0x00,0x00,0x00,0x00,0x70,0xEF},
 {0xEF,0x70,0x00,0x00,0x00,0x00,0x70,0xEF},
 {0xFF,0x9F,0x20,0x00,0x00,0x20,0x9F,0xFF},
 {0xFF,0xDF,0x9F,0x70,0x70,0x9F,0xDF,0xFF},
 {0xFF,0xFF,0xFF,0xEF,0xEF,0xFF,0xFF,0xFF}};

static uint8_t sprocket[8][8] =
{{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xEF,0xEF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xCF,0x80,0x80,0xCF,0xFF,0xFF},
 {0xFF,0xEF,0x80,0x10,0x10,0x80,0xEF,0xFF},
 {0xFF,0xEF,0x80,0x10,0x10,0x80,0xEF,0xFF},
 {0xFF,0xFF,0xCF,0x80,0x80,0xCF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xEF,0xEF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};

uint8_t noHole[8][8] =
{{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};

GdkRGBA tapeColours[5] = {
    {0.8,0.68,0.68,1.0},
    {0.1,0.1,0.5,1.0},
    {0.56,0.56,1.0,1.0},
    {0.9,0.9,0.9,1.0},
    {0.31,0.84,0.42,1.0}
};


int tapeSlideX = 0;
int mousePressedAtX = 0;
int tapeWindowWidth = 0;

gulong mouseMotionWhilePressedHandlerId = 0;
gboolean motionDetected = FALSE;
gboolean resized = FALSE;


struct GdkEventConfigure {
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  gint x, y;
  gint width;
  gint height;
};

gboolean
on_tapeImageDrawingArea_configure_event(__attribute__((unused)) GtkWidget *widget,
					GdkEventConfigure  *event,
					__attribute__((unused)) gpointer   user_data)
{
    int pos,w;
    tapeWindowWidth = event->width;
    resized = TRUE;

    pos = handPosition + tapeSlideX;

    w = (tapeWindowWidth / 2) - 75;
    
    if(pos < -w)
    {
	//printf("1");
	tapeSlideX = -w - handPosition ;
    }
    return FALSE;
}


gboolean
on_tapeImageDrawingArea_draw(GtkWidget *da,
		    cairo_t *cr,
		    __attribute__((unused))gpointer udata)
    
{
    static cairo_surface_t *surface = NULL;
    static uint8_t *maskBytes;
    static int stride;
    uint8_t *maskPointer,bit;
    uint8_t *maskSrcPointer,*sourcePixels;
    int toDraw;
    static unsigned int maskSize;

    static int MAXTODRAW,MIDPOINT;  // Measured in pixels
    int index,firstPixel;
    int Position,phase,firstChar,characterNo,nn,runLength;

    if((surface != NULL) && resized)
    {
	cairo_surface_destroy(surface);
	surface = NULL;
    }
    
    if(surface == NULL)
    {
	GtkAllocation  alloc;
	gtk_widget_get_allocation(da, &alloc);
	surface = cairo_image_surface_create(CAIRO_FORMAT_A8,
					     alloc.width,alloc.height);
	maskBytes = cairo_image_surface_get_data(surface);
	stride = cairo_image_surface_get_stride(surface);
	maskSize = (unsigned int) (stride * alloc.height);
	MAXTODRAW = alloc.width - 2;
	MIDPOINT = MAXTODRAW / 2;
	MIDPOINT -= MIDPOINT & 7;
    }
    memset(maskBytes,0x00,maskSize);
    cairo_set_source_rgba(cr,0.3,0.3,0.3,1.0);
    
    cairo_paint(cr);
    index = 0;
   
    if(fileUploadBuffer != NULL)
    {
	Position = handPosition + tapeSlideX;
	toDraw = MAXTODRAW;

	// Deal with leading edge of tape
	if(Position <= MIDPOINT)
	{   // Leading edge is visible
	    firstPixel = MIDPOINT - Position;
	    toDraw -= firstPixel; 
		
	    phase = 0;
	    firstChar = 0;
	}
	else
	{   // Leading edge is off screen left
	    firstPixel = 0;
	    phase = Position % 8;
	    firstChar = (Position - MIDPOINT) / 8;
	}
	
	if( (MIDPOINT + ((int)fileLength*8) - Position) < MAXTODRAW)
	{
	    toDraw -= MAXTODRAW - (MIDPOINT + ((int)fileLength*8) - Position);
	}

	// Creat whole rows of pixels at a time
	for(int row = 0; row < (6 * HOLEWIDTH); row++)
	{
	    if((row & 7) == 0)
	    {
		index = 0;   // Reset to start of hole image
	    }

	    // guard to stop segfaults !
	    if(toDraw < 1) goto skip;

	    bit = rowToBit[row / 8];
	    characterNo = (int) firstChar;
	    /* Point at first pixel in the row */
	    maskPointer = &maskBytes[(int)row * stride];
	    maskPointer += firstPixel;

	    runLength = toDraw;

	    if(phase != 0)
	    {
		// Or in top bit to force a sprocket hole
		if((0x80 | fileUploadBuffer[characterNo]) & bit)
		{	
		    if((row / 8) != 3)
		    {
			maskSrcPointer = &hole[index][phase];
		    }
		    else
		    {
			maskSrcPointer = &sprocket[index][phase];
		    }  
		}
		else
		{
		    maskSrcPointer = &noHole[index][phase];
		}

		nn = 8 - (int) phase;
		runLength -= nn;
		while(nn--)
		    *maskPointer++ =  *maskSrcPointer++;

		characterNo += 1;
	    }

	    // get pixel pattern from data or sprocket hole image
	    if((row / 8) != 3)
	    {
		sourcePixels  = &hole[index][0];
	    }
	    else
	    {
		sourcePixels  = &sprocket[index][0];
	    }

	    nn = runLength & 7;

	    // Draw groups of 8 pixels.  Could use a uint64_t pointer ?
	    runLength >>= 3;
	    	    
	    while(runLength--)
	    {
		if((0x80 | fileUploadBuffer[characterNo]) & bit)
		{	
		    maskSrcPointer = sourcePixels;
		}
		else
		{
		    maskSrcPointer = &noHole[index][0];
		}
		
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;
		*maskPointer++ = *maskSrcPointer++;

		characterNo += 1;
	    }

	    // get pixel pattern from data or sprocket hole image
	    if((0x80 | fileUploadBuffer[characterNo]) & bit)
	    {	
		if((row / 8) != 3)
		{
		    maskSrcPointer = &hole[index][0];
		}
		else
		{
		    maskSrcPointer = &sprocket[index][0];
		}
	    }
	    else
	    {
		maskSrcPointer = &noHole[index][0];
	    }
	
	    while(nn--)
		*maskPointer++ = *maskSrcPointer++;
skip:
	    index += 1;
	}
    }
    else
    {
	// Clear the mask to all transparent if no tape in hand
	memset(maskBytes,0x00,(unsigned)stride*48);
    }

    // Now do the drawing
    cairo_surface_mark_dirty(surface);
    if(fileUploadBuffer != NULL) gdk_cairo_set_source_rgba(cr,&tapeColours[3]);
    cairo_mask_surface(cr, surface, 0, 0);
    cairo_fill(cr);
  
    return FALSE;
}



gboolean
mouseMotionWhilePressed (__attribute__((unused)) GtkWidget      *tape,
			 __attribute__((unused)) GdkEventMotion *event,
			 __attribute__((unused)) gpointer        data)
{
    int x,pos,w;

    x = (int) event->x;

    if(!motionDetected)
    {
	motionDetected = TRUE;
    }

    tapeSlideX = mousePressedAtX  - x;
    pos = handPosition + tapeSlideX;

    w = (tapeWindowWidth / 2) - 75;
    
    if(pos < -w)
    {
	tapeSlideX = -w - handPosition ;
    }
    if(pos > (((signed) fileLength*8)+200) )
    {
	tapeSlideX = -(handPosition) + ((int)fileLength*8)+200;
    }

    gtk_widget_queue_draw(tape);
    return FALSE;
}


gboolean
on_tapeImageDrawingArea_button_press_event(__attribute__((unused))GtkWidget *tape,
				  __attribute__((unused))GdkEventButton *event,
				  __attribute__((unused))gpointer data)
{
    mousePressedAtX = (int) event->x;
    tapeSlideX = 0;

    mouseMotionWhilePressedHandlerId =
	g_signal_connect (G_OBJECT (tape), 
			  "motion_notify_event",
			  G_CALLBACK (mouseMotionWhilePressed), 
			  NULL);
    motionDetected = FALSE;

    return FALSE;
}

gboolean
on_tapeImageDrawingArea_button_release_event(__attribute__((unused))GtkWidget *tape,
				    __attribute__((unused))GdkEventButton *event,
				    __attribute__((unused))gpointer data)
{
    /* Disable motion event handler if installed*/
    if(mouseMotionWhilePressedHandlerId != 0)
    {
	g_signal_handler_disconnect (G_OBJECT (tape),
				     mouseMotionWhilePressedHandlerId );
	mouseMotionWhilePressedHandlerId  = 0;
    }

    handPosition += tapeSlideX;
    tapeSlideX = 0;

    return FALSE;
}


/******************************* Communications ****************************/

#if 0
// Code and definitions used for debuging

#define GLIB_SYSDEF_POLLIN =1
#define GLIB_SYSDEF_POLLOUT =4
#define GLIB_SYSDEF_POLLPRI =2
#define GLIB_SYSDEF_POLLHUP =16
#define GLIB_SYSDEF_POLLERR =8
#define GLIB_SYSDEF_POLLNVAL =32
typedef enum
{
  G_IO_STATUS_ERROR,
  G_IO_STATUS_NORMAL,
  G_IO_STATUS_EOF,
  G_IO_STATUS_AGAIN
} GIOStatus;
typedef enum /*< flags >*/
{
  G_IO_IN       GLIB_SYSDEF_POLLIN,
  G_IO_OUT      GLIB_SYSDEF_POLLOUT,
  G_IO_PRI      GLIB_SYSDEF_POLLPRI,
  G_IO_ERR      GLIB_SYSDEF_POLLERR,
  G_IO_HUP      GLIB_SYSDEF_POLLHUP,
  G_IO_NVAL     GLIB_SYSDEF_POLLNVAL
} GIOCondition;

static void
printGIOCondition(GIOCondition condition)
{

    printf("condition = 0x%x ",condition);
    if(condition & G_IO_IN)   printf("G_IO_IN ");
    if(condition & G_IO_OUT)  printf("G_IO_OUT ");
    if(condition & G_IO_PRI)  printf("G_IO_PRI ");
    if(condition & G_IO_ERR)  printf("G_IO_ERR ");
    if(condition & G_IO_HUP)  printf("G_IO_HUP ");
    if(condition & G_IO_NVAL) printf("G_IO_NVAL ");
   
}

static void
printGIOStatus(GIOStatus status)
{
    printf("status = 0x%x ",status);
    switch(status)
    {
    case  G_IO_STATUS_ERROR:
	printf("G_IO_STATUS_ERROR ");
	break;
    case  G_IO_STATUS_NORMAL:
	printf("G_IO_STATUS_NORMAL ");
	break;
    case  G_IO_STATUS_EOF:
	printf("G_IO_STATUS_EOF ");
	break;
    case  G_IO_STATUS_AGAIN:
	printf("G_IO_STATUS_AGAIN ");
	break;
    default:
	printf("UNKNOWN ");
	break;
    }
}
#endif

static gboolean
readPTSHandler(guchar rdChar)
{
    gchar writeChar;
    //gchar *cp;
    gsize cnt; //,n;
    const gchar *buffer;
    GtkTextMark *mark;
    GtkTextIter enditer;
    int finished;
    gsize written;
    GError *error = NULL;
    GtkTextView *textview;
    GtkTextBuffer *textbuffer;
    static gboolean pletters = FALSE;
    static gboolean pfigures = FALSE;
    static gboolean rletters = FALSE;
    static gboolean rfigures = FALSE;
    static gboolean *letters;
    static gboolean *figures;
    gunichar txt;
    guint readChar;
    gchar utf8Ch[6];
    gint utf8ChLength;
    int type;

    type = -1;
    readChar = rdChar;
    //printf("readChar = %02X \n",readChar & 0xFF);

    txt = 0;
    if(readChar < 0x80)
    {
	// Changed from 0x20 to allow 6 bit EDSAC characters to be echoed.
	if(readChar < 0x40)
	{
	    // Teleprinter Output
	    readChar &= 0x1F;
	    // Set type to -1 to ignore runouts else 3 for Elliott telecode.
	    type = (readChar == 0) ? -1 : 3; 
	    if(printing == TRUE)
	    {
		textview = teleprinterTextView;
		textbuffer = teleprinterTextBuffer;
	    }
	    else
	    {
		textview = NULL;
		textbuffer = NULL;
	    }
	    letters = &pletters;
	    figures = &pfigures;

	    if(punching)
	    {
		g_byte_array_append(punchingBuffer,&rdChar,1);
	    }
	}
	else
	{
	    // Reader Echo
	   
	    textbuffer = readerTextBuffer;
	    textview = readerTextView;
	    letters = &rletters;
	    figures = &rfigures;
	    //printf("Setting type to %d ",readerEchoType);
	    // Set type to 8 to enable an echoed CR to print "\n"
	    if(readerOnline)
		type = 8;
	    else
		type = readerEchoType;
	}

	
	//if(ELLIOTTcode)
	if((type == 3) || (type == 4) || (type == 8))
	{
	    readChar &= 0x1F;
	    //printf("ELLIOTT Echo 0x%02X\n",readChar);
	    if(readChar >= 0x1B)
	    {
		switch(readChar)
		{
		case 0x1B:
		    *figures = TRUE;
		    *letters = FALSE;
		    txt = 0;
		    break;
		case 0x1C:
		    txt = U' ';
		    break;
		case 0x1D:
		    // If reader online make CR print "\n"
		    if(type == 8)
			txt = U'\n';
		    else
			txt = 0;
		    break;
		case 0x1E:
		    txt = U'\n';
		    break;
		case 0x1F:
		    *figures = FALSE;
		    *letters = TRUE;
		    break;
		}
	    }
	    else
	    {
		// Changed to let initial runouts through
		//if( *letters || *figures)
		{
		    if(*letters) readChar += 32;
		    txt = ElliottToUnicode[readChar];
		}
	    }
	}

	//if(EDSACcode)
	if((type == 5) || (type ==6))
	{
	    static int count = 0;
	    readChar &= 0x3F;
	    // Echoed chars have the original FS/LS state in bit six
	    //printf("EDSAC Echo 0x%02X\n",readChar);
	    txt = EDSACToUnicode[readChar];

	    if(readChar != 0x10) count += 1;
	    if(count == 32)
	    {
		count = 0;
		mark = gtk_text_buffer_get_mark (textbuffer, "end");
		gtk_text_buffer_get_iter_at_mark (textbuffer,&enditer,mark);
		gtk_text_buffer_insert(textbuffer,&enditer,"\n",1);
	    }
	    
	}
/*
	if(MURRYcode)
	{
	    reversed = ((readChar & 1) << 4) +((readChar & 2) << 2) + (readChar & 4) +
		((readChar & 8) >> 2) + ((readChar & 16) >> 4);
	    
	    switch(reversed)
	    {
	    case 0x02:
		txt = "\n";
		break;

	    case 0x08:
		txt = "<cr>";
		break;
	    case 0x1B:
		    *figures = TRUE;
		    *letters = FALSE;
		break;
	    case 0x1F:
		    *figures = FALSE;
		    *letters = TRUE;
		break;
	    default:
		if( *letters || *figures)
		{
		    if(*letters) reversed += 32;
		    txt = convert3[reversed];
		}
		break;
	    }
	}
*/	
	if( (txt != 0) && (textview != NULL)) 
	{
	    mark = gtk_text_buffer_get_mark (textbuffer, "end");
	    gtk_text_buffer_get_iter_at_mark (textbuffer,&enditer,mark);

	    utf8ChLength = g_unichar_to_utf8(txt,utf8Ch);
	    
	    gtk_text_buffer_insert(textbuffer,&enditer,utf8Ch,utf8ChLength);
	    
	    gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW(textview), mark);
	}
    }
    else
    {   // char >= 0x80
	switch(readChar)
	{
	case 0x81:
	    finished = FALSE;

	    cnt = *uploadLengthp - *uploadedp;
	    if( cnt > 256)
	    {
		cnt = 256;
	    }

	    buffer = &uploadBuffer[*uploadedp];
	    
	    if(cnt > 0)
	    {
		writeChar = '\x81';
		g_io_channel_write_chars(E803_channel,&writeChar,1,&written,&error);
		writeChar = (gchar) (cnt & 0xFF);
		g_io_channel_write_chars(E803_channel,&writeChar,1,&written,&error);
		
		g_io_channel_write_chars(E803_channel,buffer,(gssize)cnt,&written,&error);
		g_io_channel_flush(E803_channel,NULL);
		*uploadedp += cnt;
	    }
	    else
	    {
		finished = 1;
	    }

	    if(*uploadLengthp != 0)
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(uploadProgressBar),
					      (gdouble)*uploadedp/(gdouble)*uploadLengthp);
	    
	    if(finished)
	    {
		writeChar = '\x82';
		g_io_channel_write_chars(E803_channel,&writeChar,1,&written,&error);
		g_io_channel_flush(E803_channel,NULL);
				
		// Restore button states
		gtk_widget_set_sensitive(fileUploadButton,fileUploadWasSensitive);
		gtk_widget_set_sensitive(editorUploadButton,editorUploadWasSensitive);
		gtk_widget_set_sensitive(editorUploadFromCursorButton,editorUploadFromCursorWasSensitive);
	    }
	    break;
	default:
	    break;
	}
    }

    return TRUE;
}


/* 
The socket used for a network connection to an emulator is set non-blocking 
(see SetSocketNonBlockingEnabled) before the call to connect().  This means that connect()
returns immidiatley with errno set to EINPROGRESS which perror prints as "Operation now in progress".

There are three "watches" set up on "E803_channel" (see on_networkConnectButton_clicked()).
E803_errorHandler       G_IO_ERR | G_IO_HUP     Error or Hung Up
E803_messageHandler     G_IO_IN                 Readable
E803_connectedHandler   G_IO_OUT                Writeable

E803_connectedHandler is only used once when the channel first becomes writeable after it is created.
If this is called it means that the network connection has succeeded.  It returns FALSE to remove the watch
otherwise it is repeatedly called.

E803_errorHandler is called if connect() eventually fails.  It is called with condition set to G_IO_ERR and G_IO_HUP.

E803_messageHandler is called when there is incomming data to read on the channel.  It is also called 
if the other end of the socket is closed.  In this case the call to g_io_channel_read_chars() returns
the staus "G_IO_STATUS_EOF". 


 */


static gboolean
E803_messageHandler(GIOChannel *source,
			     __attribute__((unused)) GIOCondition condition,
			     __attribute__((unused)) gpointer data)
{
    guchar message;
    gsize length;
    GError *error = NULL;
    GIOStatus status;
 
    status = g_io_channel_read_chars(source,(gchar *)&message,1,&length,&error);
  
    if(status != G_IO_STATUS_NORMAL)
    {
	// Remove the error source
	g_source_remove(ErWatchId);
	g_io_channel_shutdown(source,FALSE,NULL);
	g_io_channel_unref(E803_channel);
	E803_channel = NULL;
	// Returning FALSE will remove the recieve source

	gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-connect");
	reconnectButtonState = FALSE;
	gtk_window_set_title(GTK_WINDOW(mainWindow),"Disconnected");
	gtk_widget_set_sensitive(editorUploadButton,FALSE);
	gtk_widget_set_sensitive(editorUploadFromCursorButton,FALSE);
	gtk_widget_set_sensitive(fileUploadButton,FALSE);
	return FALSE;
    }
 
    readPTSHandler(message);
   
    return TRUE;
}


static gboolean
E803_connectedHandler( __attribute__((unused)) GIOChannel *source,
		       __attribute__((unused)) GIOCondition condition,
		       __attribute__((unused)) gpointer data)
{

    // When the connection is made, make it look like the user pressed the non-existent
    // "OK" button on the makingConnectionDialog.
    gtk_dialog_response(GTK_DIALOG(makingConnectionDialog),GTK_RESPONSE_OK);


    gtk_widget_set_sensitive(editorUploadButton,TRUE);
    gtk_widget_set_sensitive(editorUploadFromCursorButton,TRUE);
    if(fileUploadBuffer != NULL)
	gtk_widget_set_sensitive(fileUploadButton,TRUE);
    //gtk_widget_set_sensitive(fileUploadButton,FALSE);
    // Handler has done it's work so return FALSE to remove source.

    // Send echo on/off to PLTS.
    on_readerEchoButton_toggled(readerEchoButton,NULL);
    on_readerOnlineCheckButton_toggled(GTK_BUTTON(readerOnlineCheckButton),NULL);
    
    return FALSE;
}

static gboolean
E803_errorHandler( __attribute__((unused)) GIOChannel *source,
		       __attribute__((unused)) GIOCondition condition,
		       __attribute__((unused)) gpointer data)
{
    g_source_remove(RxWatchId);
    g_source_remove(TxWatchId);
    
    g_io_channel_shutdown(source,FALSE,NULL);
    g_io_channel_unref(E803_channel);
    E803_channel = NULL;
    // When the connection fails, make it look like the user pressed the non-existent
    // "REJECT" button on the makingConnectionDialog.
    gtk_dialog_response(GTK_DIALOG(makingConnectionDialog),GTK_RESPONSE_REJECT);
    
    // Handler has done it's work so return FALSE to remove source.
    return FALSE;
}


/* Returns true on success, or false if there was an error */
static gboolean
SetSocketNonBlockingEnabled(int fd, gboolean blocking)
{
   if (fd < 0) return FALSE;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) return FALSE;
   flags = blocking ? (flags | O_NONBLOCK) :  (flags & ~O_NONBLOCK) ;
   return (fcntl(fd, F_SETFL, flags) == 0) ? TRUE : FALSE;

}



gboolean
on_networkConnectButton_clicked(__attribute__((unused)) GtkButton *button,
				     __attribute__((unused)) gpointer data)
{
    GString *address,*title;
    int E803_socket,n,result;
    struct hostent *hp;
    struct sockaddr_in E803_server_name;
    gboolean localHost = FALSE;

    localHost = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useLocalHost));
    
    address = g_string_new(NULL);

    if(localHost)
    {
	g_string_printf(address,"localhost");
    }
    else
    {
	g_string_printf(address,"%d.%d.%d.%d",
			(int)gtk_adjustment_get_value(ipAdjustments[0]),
			(int)gtk_adjustment_get_value(ipAdjustments[1]),
			(int)gtk_adjustment_get_value(ipAdjustments[2]),
			(int)gtk_adjustment_get_value(ipAdjustments[3]));
    }

    /* Create socket on which to send and recieve. */
    E803_socket = socket(AF_INET, SOCK_STREAM, 0); 
    if (E803_socket < 0) { 
	perror("opening network socket to the E803");
	g_string_free(address,TRUE);
	return GDK_EVENT_PROPAGATE ; 
    } 

    SetSocketNonBlockingEnabled(E803_socket,TRUE);
    
    hp = gethostbyname(address->str); 
    if (hp == 0) { 
	fprintf(stderr, "%s: unknown host", address->str);
	g_string_free(address,TRUE);
	return GDK_EVENT_PROPAGATE ; 
    } 
    
    bcopy(hp->h_addr, &E803_server_name.sin_addr, (size_t) hp->h_length); 
    E803_server_name.sin_family = AF_INET; 
    E803_server_name.sin_port = htons(8038);  ; 
    
    n = connect(E803_socket, (struct sockaddr *)&E803_server_name,
	      sizeof (E803_server_name));
  
    if(n == -1)
    {
	perror("E803 connect returned:");
    }

    E803_channel = g_io_channel_unix_new(E803_socket);
    ErWatchId = g_io_add_watch(E803_channel,G_IO_ERR | G_IO_HUP ,E803_errorHandler,NULL);
    RxWatchId = g_io_add_watch(E803_channel,G_IO_IN ,E803_messageHandler,NULL);
    TxWatchId = g_io_add_watch(E803_channel,G_IO_OUT ,E803_connectedHandler,NULL);

    g_io_channel_set_encoding(E803_channel,NULL,NULL);
    // 24/11/18
    g_io_channel_set_buffered(E803_channel,FALSE);

    result = gtk_dialog_run (GTK_DIALOG (makingConnectionDialog));
    switch(result)
    {
    case GTK_RESPONSE_NONE:
	break;
	
    case GTK_RESPONSE_REJECT:
	// From error handler when connection fails.  Already tidied up.
	break;
    case GTK_RESPONSE_ACCEPT:
	break;
    case GTK_RESPONSE_DELETE_EVENT:
	break;
    case GTK_RESPONSE_OK:
	// From connected handler.
	gtk_widget_hide(connectionWindow);
	if(!gtk_widget_get_visible(mainWindow))
	{
	     gtk_widget_show(mainWindow);
	}
	title = g_string_new(NULL);
	g_string_printf(title,"Connected to %s",address->str);
	gtk_window_set_title(GTK_WINDOW(mainWindow),title->str);
	g_string_free(title,TRUE);
				 
	gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-disconnect");
	reconnectButtonState = TRUE;
	break;
    case GTK_RESPONSE_CANCEL:
	// User has aborted th econnection so need to tidy up here.
	g_source_remove(RxWatchId);
	g_source_remove(TxWatchId);
	g_source_remove(ErWatchId);
	g_io_channel_shutdown(E803_channel,FALSE,NULL);
	g_io_channel_unref(E803_channel);
	E803_channel = NULL;
	break;
    case GTK_RESPONSE_CLOSE:
	break;
    case GTK_RESPONSE_YES:
	break;
    case GTK_RESPONSE_NO:
	break;
    case GTK_RESPONSE_APPLY:
	break;
    case GTK_RESPONSE_HELP:
	break;
    default:
	break;
    }
   
    g_string_free(address,TRUE);
    gtk_widget_hide(makingConnectionDialog);
    
    return GDK_EVENT_PROPAGATE ;
}


static
void populateSerialList(GtkBuilder *builder)
{
    int n;
    char *realname,*base;
    GString *fullpath;
    GtkListStore *store;
    GtkTreeIter iter;
    glob_t globbuf;

    store = GTK_LIST_STORE(gtk_builder_get_object_checked (builder, "serialDeviceListStore"));
	
    fullpath = g_string_new(NULL);

    // Find all the ttyUSB* and ttyS* devices in /dev
    globbuf.gl_offs = 0;
    glob("/dev/ttyUSB*", GLOB_DOOFFS, NULL, &globbuf);
    glob("/dev/ttyS*", GLOB_DOOFFS | GLOB_APPEND, NULL, &globbuf);

    n = 0;
    while(globbuf.gl_pathv[n] != NULL)
    {
	// Workout if there is real hardware for each device...

	g_string_printf(fullpath,"/sys/class/tty/%s/device/subsystem",&globbuf.gl_pathv[n][5]);
	realname  = realpath(fullpath->str,NULL);
	base = g_path_get_basename(realname);

	// If value is "platform" it is NOT a real interface.
	if(strcmp(base,"platform") != 0)
	{
	    gtk_list_store_append (store, &iter);
	    gtk_list_store_set (store, &iter,
				0,globbuf.gl_pathv[n] ,
				-1);
	}
	g_free(base);
	free(realname);
	n += 1;
    }

    globfree(&globbuf);
    
    gtk_combo_box_set_active(connectionCombobox,0);
}
    


static gboolean splashOff(__attribute__((unused)) gpointer user_data)
{
    gtk_widget_hide(splashWindow);
    return G_SOURCE_REMOVE;
    
}

// Helper 
GObject *gtk_builder_get_object_checked(GtkBuilder *builder,const gchar *name)
{
    GObject *gotten;

    gotten = gtk_builder_get_object (builder,name);
    if(gotten == NULL)
    {
	g_error("FAILED TO GET (%s)\n",name);
    }
    return gotten;
}
    
// CSS to override stupid wildcard defaults set on Raspberry Pi Desktop.
// burt,harry,john,fred at the names of the four GtkFrames which contain all the 
// editor, reader echo, teleprinter and file upload widgets respectivly. 

const gchar *css1 =
"\
    textview {\
    font-family: monospace;\
    font-size: 10pt;\
    font-weight: bold;\
}\
  #burt   {\
  background-color: #FFFFE0;\
  color: black;\
}\
  #burt text   {\
  background-color: #FFFFE0;\
  color: black;\
}\
  #burt button   {\
  background-color: #FFFFE0;\
  color: black;\
}\
  #burt button:disabled   {\
  background-color: #C0C080;\
  color: black;\
}\
  #harry   {\
  background-color: #E0E0FF;\
  color: black;\
}\
  #harry text   {\
  background-color: #E0E0FF;\
  color: black;\
}\
  #harry button   {\
  background-color: #E0E0FF;\
  color: black;\
}\
  #harry button:disabled   {\
  background-color: #8080C0;\
  color: black;\
}\
  #fred   {\
  background-color: #E0FFE0;\
  color: black;\
}\
  #fred text   {\
  background-color: #E0FFE0;\
  color: black;\
}\
  #fred button   {\
  background-color: #E0FFE0;\
  color: black;\
}\
  #john   {\
  background-color: #FFE0E0;\
  color: black;\
}\
  #john text   {\
  background-color: #FFE0E0;\
  color: black;\
}\
  #john button   {\
  background-color: #FFE0E0;\
  color: black;\
}\
  #john button:disabled   {\
  background-color: #C08080;\
  color: black;\
}\
";




// Command line options

static gchar *windowSize = NULL;

static GOptionEntry entries[] =
{

    { "windowsize", 'w', 0, G_OPTION_ARG_STRING, &windowSize, "Window size as widthxheight.", NULL },
    { NULL }
};




int main(int argc,char **argv)
{
    struct passwd *pw;
    uid_t uid;
    gboolean createConfigDirectory = FALSE;
    int  a1,a2,a3,a4;
    gboolean addressOK = FALSE;
    
    GtkBuilder *builder;
    GString *adjustmentName;
    int n;
    GtkTextIter iter;
    GdkDisplay *display;
    GdkScreen *screen;
    GtkCssProvider *provider;
    GError *error = NULL;
    GOptionContext *context;
    gboolean windowSizeOk = FALSE;
    int windowWidth,windowHeight;


    // New command line option parsing....
    context = g_option_context_new ("- Elliott 803 Emulator");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
    }

    
    
    if(windowSize != NULL)
    {
	if( (sscanf(windowSize,"%dx%d",&windowWidth,&windowHeight) == 2) &&
	    (windowWidth > 1040) && (windowHeight > 400))
	{
	    
	    windowSizeOk = TRUE;
	    g_info("Window size set to %d x %d\n",windowWidth,windowHeight);
	}
	else
	{
	    g_warning("%s is not a valid window size.\n",windowSize);
	}
    }

  
    gtk_init (&argc, &argv);

    // Install simple logging to stdout.
    LoggingInit();
    
    /* Set global path to user's configuration and machine state files */
    uid = getuid();
    pw = getpwuid(uid);

    configPath = g_string_new(pw->pw_dir);
    configPath = g_string_append(configPath,"/.PLTS/");

    // Now Check it exists.   If it is missing it is not an
    // error as it may be the first time this user has run the emulator.
    {
	GFile *gf = NULL;
	GFileType gft;
	GFileInfo *gfi = NULL;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(configPath->str);
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_warning("Could not read user configuration directory: %s\n", error2->message);
	    createConfigDirectory = TRUE;
	}
	else
	{
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_DIRECTORY)
	    {
		g_warning("User's configuration directory (%s) is missing.\n",configPath->str);
		createConfigDirectory = TRUE;
	    }
	}
	if(gfi) g_object_unref(gfi);
	if(gf) g_object_unref(gf);
    }

    // Find out where the executable is located.
    {
	GFile *gf;
	GFileType gft;
	GFileInfo *gfi;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path("/proc/self/exe");
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_error("Could not read /proc/self/exe: %s\n", error2->message);
	}
	else
	{
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_SYMBOLIC_LINK)
	    {
		const char *exename = g_file_info_get_symlink_target(gfi);
		g_info("exename is (%s)\n",exename);

		if(strcmp(PROJECT_DIR"/PLTS4",exename)==0)
		{
		    g_info("Running in the build tree, using local resources.\n");
		    /* Set global path to local icons, pictures and sound effect files */
		    sharedPath = g_string_new(PROJECT_DIR"/803-Resources/");		
		}
		else
		{
		    g_info("Running from installed file, using shared resources.\n");
		    /* Set global path to shared icons, pictures and sound effect files */
		    sharedPath = g_string_new("/usr/local/share/803-Resources/");
		}
	    }
	}
	g_object_unref(gfi);
	g_object_unref(gf);
    }

    // Check that the resources directory exists.
    {
	GFile *gf;
	GFileType gft;
	GFileInfo *gfi;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(sharedPath->str);
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_error("Could not read Resources directory:%s\n", error2->message);
	}
	else
	{
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_DIRECTORY)
		g_warning("803 Resources directory (%s) is missing.\n",sharedPath->str);
	}
	g_object_unref(gfi);
	g_object_unref(gf);
    }

    // Once a tape store is added here is where a default set of tapes for a user
    // will need to be created if the users config directory is missing.

    if(createConfigDirectory == TRUE)
    {
	GFile *gf;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(configPath->str);
	
	g_file_make_directory (gf,
                       NULL,
                       &error2);
	
	if(error2 != NULL)
	{
	    g_error("Could not create  directory:%s\n", error2->message);
	}

	g_object_unref(gf);
    }
    else
    {
	GString *configFileName;
	GIOChannel *file;

	GIOStatus status;
	gchar *message;
	gsize length,term;

	configFileName = g_string_new(configPath->str);
	g_string_append(configFileName,"DefaultIP");

	if((file = g_io_channel_new_file(configFileName->str,"r",&error)) == NULL)
	{
	    g_warning("failed to open file %s due to %s\n",configFileName->str,error->message);
	}
	else
	{
	    while((status = g_io_channel_read_line(file,&message,&length,&term,&error)) 
		  == G_IO_STATUS_NORMAL)
	    {
		if(message != NULL)
		{
		    g_info("read %s from congif file\n",message);

		    if(sscanf(message,"%d.%d.%d.%d\n",&a1,&a2,&a3,&a4) == 4)
		    {
			g_info("Parsed as %d %d %d %d\n",a1,a2,a3,a4);
			addressOK = TRUE;
		    }
		    else
		    {
			g_info("Failed to parse default address from %s\n",message);
		    }
	
		    g_free(message);
		}
	    }
	    g_io_channel_shutdown(file,FALSE,NULL);
	    g_io_channel_unref(file);
	}
    }

    recentManager = gtk_recent_manager_get_default ();


    {
	GString *gladeFileName;
	error = NULL;
	
	
	gladeFileName = g_string_new(sharedPath->str);
	g_string_append(gladeFileName,"PLTS4.glade");
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file(builder, gladeFileName->str, &error) == 0)
	{
	    g_info("Failed to open glade file (%s)\n", gladeFileName->str);
	    return(0);
	}
	else
	{
	    printf("OPened %s OK\n",gladeFileName->str);
	}
	
	
	
	g_string_free(gladeFileName,TRUE);


    }
    // Set globals for widgets.
    GETWIDGET(connectionCombobox,GTK_COMBO_BOX);
    GETWIDGET(connectionWindow,GTK_WIDGET);
    GETWIDGET(mainWindow,GTK_WIDGET); 

    adjustmentName =  g_string_new(NULL);
    for(n=0;n<4;n++)
    {
	g_string_printf(adjustmentName,"adjustment%d",n+1);
	
	ipAdjustments[n] = GTK_ADJUSTMENT(gtk_builder_get_object_checked (builder, adjustmentName->str));
    }
    g_string_free(adjustmentName,TRUE);

    // Set address widgets to the default address.
    if(addressOK)
    {
	gtk_adjustment_set_value(ipAdjustments[0],(gdouble)a1);
	gtk_adjustment_set_value(ipAdjustments[1],(gdouble)a2);
	gtk_adjustment_set_value(ipAdjustments[2],(gdouble)a3);
	gtk_adjustment_set_value(ipAdjustments[3],(gdouble)a4);
    }

    GETWIDGET(makingConnectionDialog,GTK_WIDGET);
    GETWIDGET(reconnectButton,GTK_WIDGET);

    GETWIDGET(openRecentFileChooserDialog,GTK_WIDGET);
    GETWIDGET(fileUploadButton,GTK_WIDGET);
    GETWIDGET(fileUploadFrameLabel,GTK_WIDGET);
    GETWIDGET(fileTooBigDialog ,GTK_WIDGET);
    GETWIDGET(notTelecodeDialog,GTK_WIDGET);
    GETWIDGET(chooseFormatDialog,GTK_WIDGET);
    GETWIDGET(tapeImageDrawingArea,GTK_WIDGET);
    GETWIDGET(readerEchoButton,GTK_WIDGET);
    GETWIDGET(readerOnlineCheckButton,GTK_WIDGET);
    GETWIDGET(editorUploadButton,GTK_WIDGET);
    GETWIDGET(editorUploadFromCursorButton,GTK_WIDGET);    
    GETWIDGET(loadFileChooserDialog,GTK_WIDGET);
    GETWIDGET(teleprinterTextView,GTK_TEXT_VIEW);
    GETWIDGET(teleprinterTextBuffer,GTK_TEXT_BUFFER);
    GETWIDGET(readerTextView,GTK_TEXT_VIEW);
    GETWIDGET(readerTextBuffer,GTK_TEXT_BUFFER);

    gtk_text_buffer_get_end_iter(teleprinterTextBuffer, &iter);
    gtk_text_buffer_create_mark(teleprinterTextBuffer, "end", &iter, FALSE);

    gtk_text_buffer_get_end_iter(readerTextBuffer, &iter);
    gtk_text_buffer_create_mark(readerTextBuffer, "end", &iter, FALSE);



    
    GETWIDGET(uploadProgressBar,GTK_WIDGET);

    GETWIDGET(windUpFromStartButton,GTK_WIDGET);
    GETWIDGET(windUpFromEndButton,GTK_WIDGET);
    GETWIDGET(discardTapeButton,GTK_WIDGET);
    GETWIDGET(saveFileChooserDialog,GTK_WIDGET);
    GETWIDGET(punchingToTapeButton,GTK_WIDGET);

    GETWIDGET(editorTextBuffer,GTK_TEXT_BUFFER);
    GETWIDGET(editorTextView,GTK_TEXT_VIEW);
    GETWIDGET(editorSaveButton,GTK_WIDGET);
    GETWIDGET(editorFrameLabel,GTK_WIDGET);
    GETWIDGET(editBinaryDialog,GTK_WIDGET);
    GETWIDGET(useLocalHost,GTK_WIDGET);
    GETWIDGET(editorELLIOTTmode,GTK_TOGGLE_BUTTON);
    GETWIDGET(editorEDSACmode,GTK_TOGGLE_BUTTON);
    GETWIDGET(deleteAbortedDialog,GTK_WIDGET);
    GETWIDGET(splashWindow,GTK_WIDGET);
    GETWIDGET(splashImage,GTK_WIDGET);
    /*
    {
	GtkTextTagTable *tagTable;
	commentTag = gtk_text_tag_new("comment");
	tagTable = gtk_text_buffer_get_tag_table (editorTextBuffer);

	gtk_text_tag_table_add (tagTable,commentTag);
    }
    */

    commentTag2 = gtk_text_buffer_create_tag (editorTextBuffer, "comment2",
					     "weight", PANGO_WEIGHT_LIGHT,
					     "foreground","#FFFF00",
					     NULL);
    
    commentTag = gtk_text_buffer_create_tag (editorTextBuffer, "comment",
					     "weight", PANGO_WEIGHT_LIGHT,
					     "foreground","#0000FF",
					     NULL);
    
    errorTag = gtk_text_buffer_create_tag (editorTextBuffer, "error",
					     "weight", PANGO_WEIGHT_NORMAL,
					     "foreground","#FF0000",
					     NULL);

    
    // Override textview style set by Raspberr Pi Desktop theme 
    display = gdk_display_get_default();
    screen = gdk_display_get_default_screen(display);
    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    error = NULL;
    //gtk_css_provider_load_from_data(provider, css,-1, &error);
    gtk_css_provider_load_from_data(provider, css1,-1, &error);
    if(error)
    {
	 g_warning("CSS error %s\n", error->message);
    }
    
    populateSerialList(builder);
 
    gtk_builder_connect_signals (builder, NULL);

    if(windowSizeOk)
	gtk_window_set_default_size(GTK_WINDOW(mainWindow),windowWidth,windowHeight);
    
/*
Solution from 
https://stackoverflow.com/questions/14430869/glib-signals-how-to-check-if-a-handler-of-an-instance-is-already-blocked

gboolean
g_signal_handlers_is_blocked_by_func(gpointer instance, GFunc func, gpointer data)
{
    return g_signal_handler_find(instance,
                                 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_UNBLOCKED,
                                 0, 0, NULL, func, data) == 0;
}

*/
    
    // Find the handler so it can be blocked when needed.
     insertHandlerId = g_signal_handler_find(editorTextBuffer,
					     G_SIGNAL_MATCH_FUNC ,
					     0, 0, NULL, on_editorTextBuffer_insert_text, NULL);
     
     deleteHandlerId = g_signal_handler_find(editorTextBuffer,
					     G_SIGNAL_MATCH_FUNC ,
					     0, 0, NULL, on_editorTextBuffer_delete_range, NULL);


     {
	 GString *splashPath;

	 splashPath = g_string_new(sharedPath->str);
	 g_string_append(splashPath,"graphics/PLTSsplash.png");
	 gtk_image_set_from_file(GTK_IMAGE(splashImage),splashPath->str);

	 g_string_free(splashPath,TRUE);
     }
     
     gtk_widget_show(splashWindow);
	      
     gtk_widget_show (connectionWindow);

     g_timeout_add(5000,splashOff,NULL);
     
     gtk_main ();
}
