import java.io.*;
import java.net.Socket;
import java.nio.file.Path;
import java.nio.file.Paths;

public class Client {
    static final int CHUNK = 1024;
    final private char[] buffer = new char[CHUNK];
    final private Socket socket;
    private final BufferedReader in;
    private final PrintWriter out;
    private final Path database = Paths.get("./database");

    public Client(Socket socket) throws IOException {
        this.socket = socket;
        in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        out = new PrintWriter(socket.getOutputStream(), true);
    }
    private String getResponse() {
        int bytesRead;
        try {
            if((bytesRead = in.read(buffer, 0, CHUNK)) != -1)return new String(buffer, 0, bytesRead);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        return null;
    }

    public String get(Path filePath){
        String response = getResponse();
        if(response.equals("Request denied."))return response;
        Path fullPath = database.resolve(filePath.getFileName());
        try{
            File file = fullPath.toFile();
            int size = Integer.parseInt(response.trim());
            FileOutputStream output = new FileOutputStream(file);
            int bytesReceived = 0;
            while(bytesReceived < size){
                String chunk = getResponse();
                if(chunk == null)break;
                output.write(chunk.getBytes());
                bytesReceived+=chunk.getBytes().length;
            }
            output.close();
            return "File received.";
        }catch(IOException e){
            e.printStackTrace();
            return "An error occurred while file transfering.";
        }
    }

    public void sendMessage(String message){
        out.println(message);
    }

    public void sendRequest(String userInput){
        String command = userInput.substring(0, userInput.indexOf(" "));
        if(command.equals("LIST") || command.equals("INFO") || command.equals("DELETE")){
            out.println(userInput);
        }else if(command.equals("PUT")){

        }else{
            out.println(userInput);
        }

    }




}
