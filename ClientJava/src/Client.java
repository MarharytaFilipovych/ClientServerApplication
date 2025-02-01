import java.io.*;
import java.net.Socket;
import java.nio.file.Files;
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

    private int getBytesRead() {
        int bytesRead;
        try {
            if((bytesRead = in.read(buffer, 0, CHUNK)) != -1)return bytesRead;
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        return -1;
    }

    private String getResponse(int bytesRead){
        return new String(buffer, 0, bytesRead);
    }

    public void outputServerResponse(){
        int bytesRead = getBytesRead();
        if(bytesRead > 0)System.out.println(getResponse(bytesRead));
        else System.out.println("No response from server!");
    }
    public void get(Path filePath){
        String response = getResponse(getBytesRead());
        if(response.equals("Request denied.")) {
            System.out.println(response);
            return;
        }
        int size=Integer.parseInt(response.trim());
        Path fullPath = database.resolve(filePath.getFileName());
        try{
            BufferedOutputStream bufferedOutputStream = new BufferedOutputStream(new FileOutputStream(fullPath.toFile()), CHUNK);
            int i = 0;
            int bytesReceived=0;
            while(i < size){
                 bytesReceived = getBytesRead();
                bufferedOutputStream.write(getResponse(bytesReceived).getBytes(), 0, bytesReceived);
                i+=bytesReceived;
            }
            bufferedOutputStream.close();
            System.out.println( "File received.");
        }catch(IOException e){
            System.out.println("An error occurred while file transferring.");
        }
    }

    public void put(Path filePath){
        File file = filePath.toFile();
        try {
            BufferedInputStream bufferedInputStream = new BufferedInputStream(new FileInputStream(file), CHUNK);
            int bytesRead;
            byte[] bufferForContent = new byte[CHUNK];
            while((bytesRead = bufferedInputStream.read(bufferForContent))!=-1){
                socket.getOutputStream().write(bufferForContent, 0, bytesRead);
            }
            socket.getOutputStream().flush();
            outputServerResponse();
        } catch (IOException e) {
            System.out.println("An error occurred while file sending.");
        }
    }

    private Path getFile(String filePart){
        String file;
        if(filePart.charAt(0)=='"' && filePart.charAt(filePart.length()-1)=='"') file=filePart.substring(1, filePart.length()-1);
        else file = filePart;
        return Paths.get(file.replace('\\', '/'));
    }
    public void sendMessage(String message){
        out.print(message);
        out.flush();
    }

    public void processRequest(String userInput){
        String[] parts = userInput.split(" ", 2);
        String command = parts[0];
        switch (command){
            case "LIST":
            case "INFO":
            case "DELETE":
                sendMessage(userInput);
                outputServerResponse();
                break;
            case "PUT":
                Path file = getFile(parts[1]);
                try{
                    if(!Files.exists(file) || !Files.isRegularFile(file) || Files.size(file)==0){
                        System.out.println("Something wrong with the file!");
                        return;
                    }
                    sendMessage(userInput + " " + Files.size(file));
                    put(file);
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
                break;
            case "GET":
                sendMessage(userInput);
                get(getFile(parts[1]));
                break;
        }
    }
}
