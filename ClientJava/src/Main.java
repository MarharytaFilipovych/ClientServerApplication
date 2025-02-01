import java.io.*;
import java.net.*;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) throws IOException {
        try {
            Socket socket = new Socket("localhost", 12345);
            Client client = new Client(socket);
            Scanner scanner = new Scanner(System.in);
            client.sendMessage("Hello, server! How are you?");
            client.outputServerResponse();
            String userInput = scanner.nextLine();
            while(true){
                if(userInput.trim().equalsIgnoreCase("EXIT")){
                    System.out.println("Client decided to terminate the connection.");
                    break;
                }
                if(Request.isInvalidRequest(userInput.trim()))System.out.println("Undefined request.");
                else if (userInput.trim().length() > 1024)System.out.println("The message length exceeds 1024 bytes, which is the maximum.");
                else client.processRequest(userInput.trim());
                userInput = scanner.nextLine();
            }
            socket.close();
            scanner.close();
        }
        catch (ConnectException e) {
            System.out.println("Connection could not be established. Ensure the server is running.");
        } catch (IOException e) {
            System.out.println("An I/O error: " + e.getMessage());
        }
    }
}